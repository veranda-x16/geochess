# geoChess for Veranda (see NOTICE)
#   make        geoChess.cvt, against an installed Veranda SDK (SDK=...)
#   make test   the test build: the scripted session linked in
#               (GC_SCRIPT=<file>, TESTFLAGS=-DGC_STOP=n ends it at frame n)
#   make run    in the emulator (the SDK's vrun)
# OUT: where the results go (default ../build/veranda)
#
# Banked overlays (Veranda's app-c-ovl.cfg; main RAM is full): record 0
# the program; record 1 overlay 1, the SDK's Open and Save dialog (its
# src/vrchoose.c), mapped while it runs (geochess.c's FileDialog); record 2
# overlay 2, the SDK's window chrome (src/vrwin.c), run in place from its
# RAM bank, which geochess.c's main maps for good as appBank. Their BSS is
# in main RAM
SDK      ?= ../build/sdk
OPT      ?= -Oirs
OUT      ?= ../build/veranda
export CC65_HOME ?= $(HOME)/.local/share/cc65
CC65     := cl65 -t none --cpu 65C02 $(OPT) -I $(SDK)/include --asm-include-dir $(SDK)/include \
            -I . -I ../src -I $(OUT)
SRC      := ../src/geochess.c vcompat.c
GC_SCRIPT ?= script_gc1.h
TEST_SRC := test/gc_test.c test/play.s
VRF      := $(OUT)/vrchoose.o $(OUT)/vrwin.o

all: $(OUT)/geoChess.cvt

$(OUT)/squares.h: ../src/GEOCHESSFONT40.cvt ../tools/mkpieces.py ../tools/cvtfont.py
	mkdir -p $(OUT)
	python3 -I -B ../tools/mkpieces.py squares $< $@

$(OUT)/vrchoose.o: $(SDK)/src/vrchoose.c $(SDK)/include/vrchoose.h
	mkdir -p $(OUT)
	cl65 -t none --cpu 65C02 -O -I $(SDK)/include --asm-include-dir $(SDK)/include \
		--code-name OVERLAY1 --rodata-name OVL1RO -c -o $@ $<

$(OUT)/vrwin.o: $(SDK)/src/vrwin.c $(SDK)/include/vrwin.h
	mkdir -p $(OUT)
	cl65 -t none --cpu 65C02 -O -I $(SDK)/include --asm-include-dir $(SDK)/include \
		--code-name OVERLAY2 --rodata-name OVL2RO -c -o $@ $<

$(OUT)/geochess.bin: $(SRC) vcompat.h ../src/geochess.h ../src/geochess-ai.h $(OUT)/squares.h $(VRF) $(EXTRA)
	$(CC65) $(CFLAGS) -C $(SDK)/cfg/app-c-ovl.cfg -m $(OUT)/geochess.map -o $@ $(SRC) $(EXTRA) $(VRF) $(SDK)/lib/veranda.lib

$(OUT)/geoChess.cvt: $(OUT)/geochess.bin icon.pbm tile.txt app.hdr
	python3 -I -B $(SDK)/tools/vpack.py --load 0x0801 -o $@ app.hdr $(OUT)/geochess.bin $(OUT)/geochess.bin.1 \
		$(OUT)/geochess.bin.2

test:
	rm -f $(OUT)/geochess.bin
	$(MAKE) OUT=$(OUT) CFLAGS="-DGC_TEST -DGC_SCRIPT='\"$(GC_SCRIPT)\"' -I $(OUT) $(TESTFLAGS)" EXTRA="$(TEST_SRC)" all

run: $(OUT)/geoChess.cvt
	$(SDK)/tools/vrun $(OUT)/geoChess.cvt

clean:
	rm -rf $(OUT)

.PHONY: all test run clean
