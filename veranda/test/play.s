; geoChess's test build: gc_play(script), Veranda's tests/sdk_play.s (MIT,
; Veranda's own test player) with one change: the frame count stands still
; while the engine thinks (_gc_think set), so a script's clicks keep their
; place whatever the engine's speed. The script (frame, x, y, flags: bit 0
; the button, bit 6 y + 256, bit 7 a key; $FFFF ends it with the last
; frame) is played
; through mouseSource with absolute positions, keys into the keyboard
; queue; at the last frame a VRAM dump and the end
.include "veranda.inc"
.include "macros.inc"
.include "x16hw.inc"

.export _gc_play
.import _gc_think

; (its zero page its own: Veranda's a0 and a1 are at $50-$53, where
; app-c-ovl.cfg puts cc65's: the port has banked overlays since Look 2)
.segment "ZEROPAGE"
sa0:	.res 2                  ; the script's next entry
sa1:	.res 2

.segment "CODE"

; a/x = the script
_gc_play:
	sta sa0
	stx sa0+1
	MoveW sa0, sa1            ; the end frame
@e:	ldy #0
	lda (sa1),y
	iny
	and (sa1),y
	cmp #$FF
	beq @e1
	AddVW 6, sa1
	bra @e
@e1:	iny
	lda (sa1),y
	sta endF
	iny
	lda (sa1),y
	sta endF+1
	LoadW mouseSource, Player
	rts

Player:	lda _gc_think
	bne @out
	IncW frame
	lda frame
	cmp endF
	bne @next
	lda frame+1
	cmp endF+1
	bne @next
	lda #1
	sta EMU_SAVE_ON_EXIT
	jmp $FFFF
@next:	ldy #0
	lda (sa0),y
	tax
	iny
	lda (sa0),y
	cmp #$FF
	beq @out
	cmp frame+1
	bcc @take
	bne @out
	cpx frame
	beq @take
	bcs @out
@take:	ldy #5
	lda (sa0),y
	bmi @key
	and #1
	sta pb
	ldy #2
	lda (sa0),y
	sta px
	iny
	lda (sa0),y
	sta px+1
	iny
	lda (sa0),y
	sta py
	iny
	lda (sa0),y              ; flags bit 6: y + 256
	and #$40
	asl
	rol
	rol
	sta py+1
	bra @adv
@key:	ldy #2
	lda (sa0),y
	jsr k_kbdbuf_put
@adv:
	AddVW 6, sa0
	bra @next
@out:
	MoveW px, r0
	MoveW py, r1
	lda pb
	sec
	rts

.segment "DATA"
endF:	.word 0
frame:	.word $FFFF
px:	.word 320
py:	.word 240
pb:	.byte 0
