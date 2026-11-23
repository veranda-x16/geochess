#!/usr/bin/env python3
"""Writes the scripted sessions the test build plays (veranda/test/gc_test.c's
P, K and END macros), and the user's moves of each for the host replay.

    gc_script.py <name> <out.h>      name: gc1 | gc2 | ...
    gc_script.py <name> --moves      the user's moves, one line
    gc_script.py <name> --saved      the moves of the game it saves
    gc_script.py <name> --name       the name it saves the game under

Frames at 60 a second; they stand still while the engine thinks (and
while a desk accessory runs). Lines "/* shot NAME FRAME */" name frames
worth a screenshot.

The menus are Veranda Look 2's menu bar (the kernel's geometry, in the
Veranda UI font: Veranda's tests/look_menu.py, from VERANDA's tree).
"""
import os
import sys

sys.path.insert(0, os.path.join(os.environ.get('VERANDA', os.path.expanduser('~/Projects/geos')), 'tests'))
import look_menu  # noqa: E402

L = []
shots = []


def P(f, x, y, b, c=None):
    L.append('P(%d, %d, %d, %d),%s' % (f, x, y, b, '  /* %s */' % c if c else ''))


def K(f, code, c=None):
    L.append('K(%d, %d),%s' % (f, code, '  /* %s */' % c if c else ''))


def click(f, x, y, c=None):
    P(f, x, y, 0, c)
    P(f + 1, x, y, 1)
    P(f + 3, x, y, 0)
    return f + 10


# the 640x480 board (geochess.h): squares of 48 x 48 from (48, 48)
def sq1(name):
    col = ord(name[0]) - ord('a')
    row = ord('8') - ord(name[1])
    return 48 + 48 * col + 24, 48 + 48 * row + 24


GC1_MOVES = "e2e4 g1f3 f1c4 b1c3 d2d3 e1g1".split()


def gc1():
    f = 0
    P(f, 320, 240, 0)
    K(40, 13, "Return: the title box's OK")
    f = 120
    for m in GC1_MOVES:
        x, y = sq1(m[:2])
        f = click(f, x, y, "%s: select" % m)
        x, y = sq1(m[2:])
        f = click(f, x, y, "%s: move (the engine answers)" % m)
        f += 30
    return f + 60


# the menu bar (geochess.h's mainMenuV, Look 2's bar): each title's
# middle, and its list's left (the bar item's, on a card); the items 16
# lines each from line 16
BAR = ['geoChess', 'File', 'Options']
MENUS = {n: ((a + b) // 2, a & ~7) for n, (a, b) in zip(BAR, look_menu.bar(BAR, ui=True))}


def menu(f, name, item, c):
    """open a menu (the title pressed, the pointer into the list the frame
    after: the list takes the pointer's region, V81), down to the item,
    press it"""
    mx, left = MENUS[name]
    x = left + 10
    P(f, mx, 8, 0, c)
    P(f + 2, mx, 8, 1)
    P(f + 3, x, look_menu.item_y(0), 1)
    P(f + 4, x, look_menu.item_y(0), 0)
    y = look_menu.item_y(item)
    P(f + 14, x, y, 0)
    P(f + 18, x, y, 1)
    P(f + 21, x, y, 0)
    return f + 90


def moves(f, ms):
    for m in ms:
        x, y = sq1(m[:2])
        f = click(f, x, y, "%s: select" % m)
        x, y = sq1(m[2:])
        f = click(f, x, y, "%s: move (the engine answers)" % m)
        f += 30
    return f


def keys(f, text, c=None):
    for i, ch in enumerate(text):
        K(f + 4 * i, ord(ch), c if i == 0 else None)
    return f + 4 * len(text) + 10


# GC2: three moves, saved through Veranda's Save dialog under a long name
# (SAVE_NAME: typed into its field, Return); two more; the saved game
# opened through the Open dialog (the initial typed: the next name
# starting with it, Return); two moves from there; geoChess > About, a desk
# accessory from the geoChess menu (Veranda's calculator: the test build
# gives it a 'q'), the colours off by Options > Colour board.
GC2_A = "e2e4 g1f3 f1c4".split()
GC2_B = "b1c3 d2d3".split()
GC2_C = "b1c3 e1g1".split()
SAVE_NAME = "Italian opening after three moves"


def gc2():
    P(0, 320, 240, 0)
    K(40, 13, "Return: the title box's OK")
    f = moves(120, GC2_A)
    shots.append(('bell', f - 33))          # just after the engine's answer
    f = menu(f, 'File', 2, "File: Save game...")
    f = keys(f + 30, SAVE_NAME, "the name, into the dialog's field")
    shots.append(('savedlg', f))
    K(f + 10, 13, "Return: Save")
    K(f + 100, 13, "Return: the \"Game saved as\" box's OK")
    f = moves(f + 160, GC2_B)
    f = menu(f, 'File', 1, "File: Open game...")
    f = keys(f + 40, SAVE_NAME[0], "its initial: the saved game selected")
    shots.append(('loaddlg', f + 10))
    K(f + 20, 13, "Return: Open")
    shots.append(('loaded', f + 120))
    f = moves(f + 140, GC2_C)
    shots.append(('colour', f))
    f = menu(f + 10, 'geoChess', 0, "geoChess: About geoChess")
    shots.append(('about', f + 10))
    K(f + 20, 13, "Return: the about box's OK")
    f = menu(f + 60, 'geoChess', 1, "geoChess: Calculator (a desk accessory)")
    f = menu(f + 120, 'Options', 0, "Options: Colour board (off)")
    return f + 120


SESSIONS = {'gc1': (gc1, GC1_MOVES), 'gc2': (gc2, GC2_A + GC2_C)}
SAVED = {'gc2': GC2_A}
NAMES = {'gc2': SAVE_NAME}


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in SESSIONS:
        sys.exit(__doc__)
    fn, moves_ = SESSIONS[sys.argv[1]]
    if sys.argv[2] == '--moves':
        print(" ".join(moves_))
        return
    if sys.argv[2] == '--saved':
        print(" ".join(SAVED[sys.argv[1]]))
        return
    if sys.argv[2] == '--name':
        print(NAMES[sys.argv[1]])
        return
    end = fn()
    with open(sys.argv[2], 'w') as o:
        o.write('/* the %s session, written by tools/gc_script.py */\n' % sys.argv[1])
        for name, frame in shots:
            o.write('/* shot %s %d */\n' % (name, frame))
        for line in L:
            o.write(line + '\n')
        o.write('END(%d)\n' % end)


if __name__ == '__main__':
    main()
