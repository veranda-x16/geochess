#!/bin/bash
# Gates of the Veranda port, in the style of Veranda's tools/gate_paint.sh.
#
#   tools/gate_veranda.sh           GEOCHESS_SD=0: skip the SD card runs
#
# Needs: Veranda (VERANDA=~/Projects/geos: its SDK is installed from it
# into build/sdk), cc65, x16emu and its ROM (EMU, ROM), Python 3 with
# Pillow for the pictures.
#
# 0. the engine's host suite (tools/hosttest.sh): perft, rules, clickpath,
#    search, selfplay, savetest
# 1. geoChess built against the installed SDK only: an application,
#    class GeoChess, $0801, VRND, 640x480, API 5, banked overlays (the
#    SDK's Open and Save dialog, its window chrome) and a tile (the one the
#    shell shows for the class, shell/tiles.txt), below the C stack
#    ($8000); it starts through the loader (AUTOSTART.cvt) and logs
#    GEOCHESS RUNNING
#
# GC1: the test build (GC_TEST, veranda/test) plays a scripted session
#    (tools/gc_script.py gc1): the title box closed with Return, then six
#    moves clicked on the board; after each the engine answers and the
#    test build writes POSITION. Checked: the position after the 12 plies
#    equals the host engine's for the same moves (veranda/test/replay.c);
#    the 64 squares on the screen equal the position drawn from
#    upstream's font (tools/check_board.py); the display shows the bitmap
#    (Veranda's tools/check_vera.py); Veranda Look 2's colours
#    (tools/check_extra.py colours: the palette, the menu bar, the board's
#    green squares and chrome frame, the move log window's title band and
#    status bar); the engine's think time per move
#    (the emulator's cycle counter); the same on an SD card image
#    (Veranda's tools/vcard.py).
# GC2: the 640x480 layout, files, the bell, colour (tools/gc_script.py
#    gc2): three moves, File > Save game... through Veranda's Save dialog
#    (vrchoose.h) under a long name ("Italian opening after three moves",
#    typed into its field: Veranda V82's longNames), two more moves, File >
#    Open game... (its initial typed in the Open dialog, Return), two
#    moves from there, geoChess > About geoChess, geoChess > Calculator (a
#    desk accessory: Veranda's, beside the game), Options > Colour board
#    (off). Checked: the dialogs' console lines (VRF NAME, the long
#    name, twice); the accessory ran and closed; the
#    positions at the save, at the load and at the end equal the host
#    engine's; the saved game's CVT is a sequential APPL_DATA file of class
#    "GeoChess Game" whose data is one SaveGame record that the host's own
#    copy of the save code (veranda/test/savecheck.c, the sources' code
#    as the host suite's savetest uses it) accepts, byte for byte the
#    record it packs from the same game, and restores to the position;
#    the bell (VERA's PSG voice 0) ringing just after the engine's answer
#    and silent at the end (tools/check_extra.py); the colour map's paper
#    per card (light and dark squares, Look 2's chrome) while the board's
#    colours are on, the squares the normal paper at the end (the chrome
#    kept);
#    the board equal to the position with the colours (every square drawn
#    light) and without (the dark squares ink); screenshots of the save
#    and load dialogs and the about box; the same on an SD card image
#    (under R49 with ROM=.../R49/rom.bin).
#
# The port needs Veranda with DECISIONS V84 (Look 2's menu bar, window
# chrome, Open and Save dialog, long names): VERANDA defaults to
# ~/Projects/geos; its HEAD is exported into build/vsrc and built there.
set -e
cd "$(dirname "$0")/.."
TOP=$(pwd)
VERANDA=${VERANDA:-$HOME/Projects/geos}
export VERANDA          # (tools/gc_script.py: tests/look_menu.py, the bar's geometry; build/vsrc below)
EMU=${EMU:-$HOME/Projects/x16-emulator/build/x16emu}
ROM=${ROM:-$HOME/Projects/x16-rom/build/x16/rom.bin}
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
PY="python3 -I -B"
OUT=build/gate
SDK=$TOP/build/sdk

ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT"

# 0. the engine on the host
tools/hosttest.sh > "$OUT/host.log" 2>&1 || { cat "$OUT/host.log"; fail "the engine's host suite"; }
ok "the engine's host suite: $(grep -c PASSED "$OUT/host.log") suites passed"
gcc -O2 -Isrc -o "$OUT/replay" veranda/test/replay.c || fail "the host replay does not build"
gcc -O2 -Isrc -Ibuild/host -o "$OUT/savecheck" veranda/test/savecheck.c || fail "the host save check does not build"

# 1. the application, against the installed SDK. Veranda's HEAD is exported
# into build/vsrc and built there (as Veranda Solitaire's gate does: its
# tree is never built in, nor its build folder read); the rest reads that
VSRC=$TOP/build/vsrc
rm -rf "$VSRC"
mkdir -p "$VSRC"
(cd "$VERANDA" && git archive HEAD) | tar -x -C "$VSRC"
(cd "$VSRC" && PATH=$CC65_HOME/bin:$PATH && $PY tools/apigen.py api/veranda.api build/gen >/dev/null \
	&& $PY tools/kbuild.py >/dev/null) || fail "Veranda does not build (in build/vsrc)"
ok "Veranda $(cd "$VERANDA" && git rev-parse --short HEAD) built in build/vsrc"
VERANDA=$VSRC
"$VERANDA/tools/mksdk.sh" "$SDK" >/dev/null || fail "the SDK does not install"
[ -f "$SDK/bin/VERANDA.PRG" ] || fail "the SDK has no VERANDA.PRG (build Veranda first)"
make -s -C veranda SDK="$SDK" OUT="$TOP/$OUT/app" >"$OUT/app.log" 2>&1 || { cat "$OUT/app.log"; fail "geoChess does not build"; }
A=$OUT/app/geoChess.cvt
field() { $PY "$SDK/tools/vpack.py" --list "$1" | sed -n "s/^$2 *//p"; }
[ "$(field "$A" 'geos type')" = 6 ] && [ "$(field "$A" class)" = 'GeoChess    V1.3' ] || fail "geoChess.cvt: type or class"
[ "$(field "$A" load)" = '$0801' ] && [ "$(field "$A" veranda)" = True ] && [ "$(field "$A" mode)" = 640x480 ] \
	|| fail "geoChess.cvt: load address, VRND or mode"
end=$(field "$A" end)
[ $((16#${end#\$})) -le $((16#8000)) ] || fail "geoChess.cvt ends at $end (the C stack is at \$8000)"
[ "$(field "$A" api)" = 5 ] && [ "$(field "$A" flags)" = 3 ] || fail "geoChess.cvt: API 5, banked overlays and a tile"
ok "geoChess builds against the installed SDK: an application, class GeoChess, \$0801-$end, VRND, 640x480, API 5, records $(field "$A" records)"
r=$($PY tools/check_extra.py tile "$A" "$VERANDA/shell/tiles.txt" geoChess) || { echo "$r"; fail "geoChess.cvt's tile"; }
ok "geoChess.cvt: ${r#check_extra: }"
# a desk accessory for the geoChess menu: Veranda's calculator (built from
# a copy of its folder: VERANDA's tree is only read)
rm -rf "$OUT/da"
cp -r "$VERANDA/apps/calc" "$OUT/da"
make -s -C "$OUT/da" SDK="$SDK" >"$OUT/da.log" 2>&1 || { cat "$OUT/da.log"; fail "the calculator does not build"; }
DA=$TOP/$OUT/da/Calculator.cvt

# run a CVT as AUTOSTART.cvt in folder $2 for at most $3 seconds; VRAM dump
# (with the calculator beside it: no /VERANDA, so the root is the system's
# folder and its APPS)
run() {
	mkdir -p "$2"
	cp "$1" "$2/AUTOSTART.cvt"
	cp "$SDK/bin/VERANDA.PRG" "$SDK/bin/VERANDA.SYS" "$DA" "$2/"
	(cd "$2" && SDL_VIDEODRIVER=dummy timeout -s INT "$3" "$EMU" -rom "$ROM" -prg VERANDA.PRG -run \
		-warp -sound none -echo -dump V > console.txt 2>&1) || true
}
# the same on an SD card image: $1 the CVT, $2 the folder, $3 seconds, then
# the files to copy out
runsd() {
	local cvt=$1 d=$2 secs=$3
	shift 3
	mkdir -p "$d/card" "$d/out"
	cp "$cvt" "$d/card/AUTOSTART.cvt"
	cp "$SDK/bin/VERANDA.SYS" "$DA" "$d/card/"
	$PY "$SDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
	cp "$SDK/bin/VERANDA.PRG" "$d/"
	(cd "$d" && SDL_VIDEODRIVER=dummy timeout -s INT "$secs" "$EMU" -rom "$ROM" -sdcard sd.img -prg VERANDA.PRG \
		-run -warp -sound none -echo -dump V > out/console.txt 2>&1) || true
	[ -f "$d/dump.bin" ] || { tail -5 "$d/out/console.txt"; fail "SD card: the session did not finish"; }
	mv "$d/dump.bin" "$d/out/"
	for f in "$@"; do
		$PY "$SDK/tools/vcard.py" get "$d/sd.img" "$f" "$d/out/$f" >/dev/null || fail "SD card: no $f"
	done
}

run "$A" "$OUT/app/run" 15
grep -aq "GEOCHESS RUNNING" "$OUT/app/run/console.txt" || fail "geoChess does not start"
ok "geoChess starts through the loader (AUTOSTART.cvt)"

# a test build of session $1 into $OUT/$1 (more make arguments after it)
testbuild() {
	local s=$1 d=$OUT/$1
	shift
	mkdir -p "$d"
	$PY tools/gc_script.py "$s" "$d/script_$s.h" || fail "the $s script"
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$d" GC_SCRIPT="script_$s.h" "$@" >"$d/build.log" 2>&1 \
		|| { cat "$d/build.log"; fail "the $s test build"; }
}

# the position the session left ($2 its folder) against the host engine's
position() {
	"$OUT/replay" $($PY tools/gc_script.py "$1" --moves) > "$2/host.txt" || fail "$1: the host engine refuses the moves"
	cmp -s "$2/host.txt" "$2/POSITION" || { diff "$2/host.txt" "$2/POSITION"; fail "$1: the position differs from the host engine's"; }
}

# think times ("TIME <move> <cycles>"): "move ms, ..."
times() {
	grep -ao 'TIME [a-h1-8]* [0-9A-F]*' "$1" | while read -r _ m c; do printf '%s %d ms, ' "$m" $((16#$c / 8000)); done
}

# a position file $3 against the host engine's for moves $1 ($2: what)
same() {
	"$OUT/replay" $1 > "$3.host" || fail "$2: the host engine refuses the moves"
	cmp -s "$3.host" "$3" || { diff "$3.host" "$3"; fail "$2: the position differs from the host engine's"; }
}
# the board on the screen: layout, dump, position [picture]
board() {
	$PY tools/check_board.py "$1" "$2" "$3" src/GEOCHESSFONT40.cvt ${4:+"$4"} >/dev/null \
		|| $PY tools/check_board.py "$1" "$2" "$3" src/GEOCHESSFONT40.cvt
}

#---------------------------------------------------------------- GC1
echo "== GC1: a game of six moves through the mouse"
testbuild gc1
R=$OUT/gc1/run
run "$OUT/gc1/geoChess.cvt" "$R" 120
[ -f "$R/dump.bin" ] || { tail -5 "$R/console.txt"; fail "GC1: the session did not finish"; }
[ "$(grep -ac 'GEOCHESS MOVED' "$R/console.txt")" = 6 ] || fail "GC1: not six engine answers"
same "$($PY tools/gc_script.py gc1 --moves)" GC1 "$R/POSITION"
ok "GC1: the position after 12 plies equals the host engine's ($(sed -n 's/^moves //p' "$R/POSITION"))"
$PY "$VERANDA/tools/check_vera.py" "$R/dump.bin" >/dev/null || fail "GC1: the display"
board gc2c "$R/dump.bin" "$R/POSITION" "$OUT/gc1-board.png" || fail "GC1: the board on the screen"
r=$($PY tools/check_extra.py colours "$R/dump.bin" on) || { echo "$r"; fail "GC1: the colours"; }
ok "GC1: the 64 squares on the screen equal the position (pixel for pixel), in their colours"
ok "GC1: the colours: ${r#check_extra: }"
$PY "$SDK/tools/vram2png.py" --color "$R/dump.bin" "$OUT/gc1.png" >/dev/null 2>&1 || true
echo "      think times (8 MHz, depth 2): $(times "$R/console.txt")"
if [ "${GEOCHESS_SD:-1}" != 0 ]; then
	runsd "$OUT/gc1/geoChess.cvt" "$OUT/gc1/sd" 120 POSITION
	same "$($PY tools/gc_script.py gc1 --moves)" "GC1 (SD card)" "$OUT/gc1/sd/out/POSITION"
	board gc2c "$OUT/gc1/sd/out/dump.bin" "$OUT/gc1/sd/out/POSITION" || fail "GC1 (SD card): the board on the screen"
	ok "GC1 on an SD card image: the same position and board"
fi

#---------------------------------------------------------------- GC2
echo "== GC2: save, load, the bell, colour"
testbuild gc2
R=$OUT/gc2/run
run "$OUT/gc2/geoChess.cvt" "$R" 180
[ -f "$R/dump.bin" ] || { tail -5 "$R/console.txt"; fail "GC2: the session did not finish"; }
SAVED=$($PY tools/gc_script.py gc2 --saved)
ALL=$($PY tools/gc_script.py gc2 --moves)
NAME=$($PY tools/gc_script.py gc2 --name)
GAME="$NAME.cvt"
# the checks of a finished GC2 session in folder $1 ($2: where)
gc2check() {
	local d=$1 w=$2
	for f in SAVEDPOS LOADED POSITION "$GAME"; do [ -f "$d/$f" ] || fail "GC2$w: no $f"; done
	same "$SAVED" "GC2$w: at the save" "$d/SAVEDPOS"
	same "$SAVED" "GC2$w: at the load" "$d/LOADED"
	same "$ALL" "GC2$w: at the end" "$d/POSITION"
	ok "GC2$w: the positions at the save, at the load and at the end equal the host engine's"
	[ "$(field "$d/$GAME" 'geos type')" = 7 ] && [ "$(field "$d/$GAME" class)" = 'GeoChess Game' ] \
		&& [ "$(field "$d/$GAME" structure)" = sequential ] || fail "GC2$w: $GAME's type, class or structure"
	"$OUT/savecheck" "$d/$GAME" $SAVED || fail "GC2$w: the saved game's format"
	ok "GC2$w: \"$GAME\" (a long name), APPL_DATA \"GeoChess Game\": the host's save code reads it as the same game"
	[ "$(grep -ac "VRF NAME $NAME" "$d/console.txt")" = 2 ] || fail "GC2$w: the Save and Open dialogs (VRF NAME $NAME, twice)"
	grep -aq "CALC RUNNING" "$d/console.txt" && grep -aq "CALC CLOSED" "$d/console.txt" || fail "GC2$w: geoChess > Calculator did not run"
	ok "GC2$w: Veranda's Save and Open dialogs gave the long name (VRF NAME, twice); geoChess > Calculator ran and closed"
	$PY "$VERANDA/tools/check_vera.py" "$d/dump.bin" >/dev/null || fail "GC2$w: the display"
	r=$($PY tools/check_extra.py colours "$d/dump.bin" off) || { echo "$r"; fail "GC2$w: the colours (the board's off)"; }
	echo "      ${r#check_extra: }"
	board gc2 "$d/dump.bin" "$d/POSITION" || fail "GC2$w: the board (no colours)"
	$PY tools/check_extra.py bell "$d/dump.bin" silent >/dev/null || fail "GC2$w: the bell at the end"
	ok "GC2$w: at the end the board equals the position without colours; the bell's voice silent"
}
gc2check "$R" ""
$PY "$SDK/tools/vram2png.py" --color "$R/dump.bin" "$OUT/gc2-mono.png" >/dev/null 2>&1 || true

# screenshots on the way ("/* shot NAME FRAME */" in the script)
sed -n 's|^/\* shot \([a-z]*\) \([0-9]*\) \*/|\1 \2|p' "$OUT/gc2/script_gc2.h" | while read -r name frame; do
	T=$OUT/gc2/shot-$name
	mkdir -p "$T"
	cp "$OUT/gc2/script_gc2.h" "$T/"
	make -s -C veranda test SDK="$SDK" OUT="$TOP/$T" GC_SCRIPT=script_gc2.h TESTFLAGS=-DGC_STOP=$frame \
		>"$T/build.log" 2>&1 || { cat "$T/build.log"; fail "the gc2 test build (stopped at $frame)"; }
	run "$T/geoChess.cvt" "$T/run" 120
	[ -f "$T/run/dump.bin" ] || fail "GC2: no dump at frame $frame"
	$PY "$SDK/tools/vram2png.py" --color "$T/run/dump.bin" "$OUT/gc2-$name.png" >/dev/null 2>&1 || true
	$PY "$VERANDA/tools/check_vera.py" "$T/run/dump.bin" >/dev/null || fail "$name: the display"
	$PY tools/check_extra.py colours "$T/run/dump.bin" on >/dev/null || { $PY tools/check_extra.py colours "$T/run/dump.bin" on; fail "$name: the colours"; }
	case $name in
	bell) $PY tools/check_extra.py bell "$T/run/dump.bin" ringing || fail "the bell" ;;
	loaded) board gc2c "$T/run/dump.bin" "$T/run/LOADED" || fail "loaded: the board" ;;
	colour) board gc2c "$T/run/dump.bin" "$T/run/POSITION" "$OUT/gc2-board.png" || fail "colour: the board" ;;
	esac
done
ok "GC2: screenshots (bell, savedlg, loaddlg, loaded, colour, about): the colours on, the bell ringing, the boards equal"
if [ "${GEOCHESS_SD:-1}" != 0 ]; then
	runsd "$OUT/gc2/geoChess.cvt" "$OUT/gc2/sd" 180 POSITION LOADED SAVEDPOS "$GAME"
	gc2check "$OUT/gc2/sd/out" " (SD card)"
fi
echo "pictures: $OUT/gc1.png (GC1's end, in colour), $OUT/gc2-mono.png (GC2's end, the board's colours off),"
echo "          $OUT/gc2-colour.png, gc2-savedlg.png, gc2-loaddlg.png, gc2-loaded.png, gc2-bell.png, gc2-about.png,"
echo "          $OUT/gc1-board.png, gc2-board.png (the board: expected | screen)"
echo "GATE geochess: PASS"
