# geoChess on Veranda

The port of geoChess to Veranda (the GEOS-style desktop for the Commander
X16): a native 640x480 application built with Veranda's C SDK. See
`../NOTICE` for the authors and terms.

    ~/Projects/geos/tools/mksdk.sh $PWD/../build/sdk  # the SDK, once
    make                    # ../build/veranda/geoChess.cvt
    make run                # in the emulator
    ../tools/hosttest.sh    # the engine's host suite (gcc)
    ../tools/gate_veranda.sh

Put `geoChess.cvt` with Veranda's applications (the SD card's
`VERANDA/APPS` folder) and double-click it.

**It needs Veranda with DECISIONS V84** (main): Veranda Look 2's menu bar
(V74, V79), window chrome and Open and Save dialog (V79), long names (V82),
API level 5. It carries its own tile (`tile.txt`: the shell's for the
class GeoChess, chrome).

## The screen (Veranda Look 3, "Sunroom")

- the menu bar: Veranda's (the kernel's, `theme_menus()`): the Veranda
  mark, **geoChess** (About geoChess; the desk accessories of
  `/VERANDA/APPS`; Quit, Ctrl Q), **File** (New game, Ctrl N; Open
  game..., Ctrl O; Save game..., Ctrl S), **Options** (Colour board),
  the clock and the speaker at the right; menus and boxes in the UI font,
  an open menu in the theme's highlight colour; in the Veranda look the
  lists show their groups (a dotted line) and each item's key at the
  right (an item's text: `$1F` starts a group, the key after a TAB);
- the theme's palette (`theme_palette()`; dark mode and the themes as
  Preferences sets them);
- the board: 48 x 48 squares (six cards each way) from (48, 48), the
  pieces upstream's font glyphs scaled 2x (Scale2x) by
  `tools/mkpieces.py` into one compacted bitmap per square and piece;
  its frame on a ring of chrome cards;
- the colour map (Options > Colour board, on at the start): light
  squares mist and dark squares sage (the Sunroom desktop's colour); in
  dark mode the light squares green, the dark ones dark sage,
  every square drawn as a light one so the cards' paper colours it; off,
  the squares are the normal paper and the dark squares ink as on GEOS
  (the pieces are always black and white);
- the move log to the right, a window of the SDK's chrome (`vrwin.h`:
  title band "Moves" in the theme's accent, a frame, a shadow), a row a
  move, the last 33 rows; numbered while the game's start is known (not
  after a load: the file keeps the last eleven plies, as upstream's);
- the status in the window's status bar (chrome).

## What changed from the GEOS version

- `vcompat.h` / `vcompat.c` map cc65's GEOS calls onto Veranda's: y-first
  argument order onto the wide calls, struct window, cc65's `DlgBoxOk`
  over `DoDlgBox`, a saved game read from its CVT file as a stream (no
  block chains; its GEOS type and class checked), Veranda's info block
  for `SaveFile`;
- saved games go through Veranda's standard Open and Save dialog
  (`vr_file`: folders, the card's places, the type filter "Saved games")
  under long names (`longNames`: up to 59 characters, the file
  `<name>.cvt`); the format is upstream's;
- banked overlays (main RAM is full): overlay 1 the SDK's Open and Save
  dialog (mapped while it runs), overlay 2 the SDK's window chrome and
  the port's drawing and menu routines (mapped for good as `appBank`);
- the board, the move log and the status are drawn for 640x480;
- the bell is VERA's PSG (voice 0, a triangle at about 1760 Hz), its decay
  done in the interrupt (`vr_set_irq`);
- clicks are presses only (Veranda calls `otherPressVec` for presses and
  releases; upstream skipped two events after a selection instead);
- the saved game format is upstream's, unchanged (the host's own save
  code reads Veranda's files: gate GC2).

The engine is unchanged; `-Oirs` (cc65) makes it about a fifth faster.
