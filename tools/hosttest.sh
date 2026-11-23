#!/bin/sh
# The engine's host test suite, built with the host compiler (the
# equivalent of test/run.sh, without running anything from upstream):
# perft, rules, clickpath, search, selfplay and savetest.
#
# savetest: the SaveGame record (src/geochess.h) and the pack, validate
# and unpack code (src/geochess.c, from "#define SAVE_PACK" up to
# LoadGameMenuHandler) are cut out with sed into build/host/save_extract.h
# and compiled into test/savetest_host.c (mksavetest.py's harness), so the
# code tested is the code the Veranda port ships.
#
#   tools/hosttest.sh            CC=clang to change the compiler
set -e
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
B=build/host
mkdir -p "$B"
{
	sed -n '/^#define SAVE_MAGIC0/,/^} SaveGame;/p' src/geochess.h
	sed -n '/^#define SAVE_PACK(sq)/,/^void LoadGameMenuHandler/p' src/geochess.c | sed '$d'
} > "$B/save_extract.h"
grep -q 'SaveDataOK' "$B/save_extract.h" || { echo "hosttest: no save code found"; exit 1; }
fails=0
for t in perft rules clickpath search selfplay savetest_host; do
	"$CC" -O2 -Isrc -I"$B" -o "$B/$t" "test/$t.c"
done
for t in perft rules clickpath search selfplay savetest_host; do
	printf '===== %s =====\n' "$t"
	if "$B/$t" > "$B/$t.log" 2>&1; then
		tail -1 "$B/$t.log"; echo "----- $t PASSED"
	else
		cat "$B/$t.log"; echo "----- $t FAILED"; fails=$((fails + 1))
	fi
done
[ "$fails" -eq 0 ] && echo "all suites passed" || { echo "$fails suite(s) failed"; exit 1; }
