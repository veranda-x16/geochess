/*
 * The geoChess test build's hooks (veranda/Makefile: make test): the
 * scripted session (play.s plays script.h through mouseSource), and the
 * position after every AI move written to the file POSITION in
 * replay.c's format, so the gate can compare it with the host engine's.
 *
 * Part of the Veranda port (see NOTICE).
 */
#include <veranda.h>
#include <string.h>

extern unsigned char board[128];
extern int side, castle_rights, ep_square;
extern unsigned char engine_state;
extern char user_move[5], ai_move[5];

extern void __fastcall__ gc_play(const void *script);

/* play.s's format: frame, x, y (low byte) | flags << 8 (bit 0 the
   button, bit 6 y + 256, bit 7 a key: x is its code). GC_STOP: the
   session ends at that frame (the entries after it become the end
   mark). Frames do not count while the engine thinks (gc_think). */
#ifndef GC_STOP
#define GC_STOP 0xFFFE
#endif
#define AT(f, x, y) ((f) > GC_STOP ? 0xFFFF : (f)), ((f) > GC_STOP ? GC_STOP : (x)), (y)
#define P(f, x, y, b) AT(f, x, ((y) & 255) | (((y) >> 8) << 14) | ((b) << 8))
#define K(f, c) AT(f, c, 0x8000)
#define END(f) 0xFFFF, ((f) > GC_STOP ? GC_STOP : (f)), 0

static const unsigned int script[] = {
#include GC_SCRIPT
};

void gc_test_start(void)
{
    gc_play(script);
}

static char moves[600];
static char line[64];
static const char hex[] = "0123456789ABCDEF";

static void put(const char *m)
{
    if (moves[0])
        strcat(moves, " ");
    strcat(moves, m);
}

static char *num(char *p, int v)
{
    char d[6];
    unsigned char n = 0;
    if (v < 0) {
        *p++ = '-';
        v = -v;
    }
    do {
        d[n++] = '0' + v % 10;
        v /= 10;
    } while (v);
    while (n)
        *p++ = d[--n];
    return p;
}

static void field(unsigned char h, const char *name, int v)
{
    char *p = line;
    strcpy(p, name);
    p = num(p + strlen(name), v);
    *p++ = '\n';
    FWrite(h, line, p - line);
}

/* the position, in replay.c's format, to the file name */
static void position(const char *name)
{
    unsigned char h, r, c, b;
    char *p;

    h = FOpen((void *)name, 'w');
    if (h < 2)
        return;
    for (r = 0; r < 8; ++r) {
        p = line;
        for (c = 0; c < 8; ++c) {
            b = board[r * 16 + c];
            *p++ = hex[b >> 4];
            *p++ = hex[b & 15];
            *p++ = c == 7 ? '\n' : ' ';
        }
        FWrite(h, line, 24);
    }
    field(h, "side ", side);
    field(h, "rights ", castle_rights);
    field(h, "ep ", ep_square);
    field(h, "state ", engine_state);
    FWrite(h, "moves ", 6);
    FWrite(h, moves, strlen(moves));
    FWrite(h, "\n", 1);
    FClose(h);
}

/* after HandleBoardClick's AI move (or none: the game over) */
void gc_test_moved(void)
{
    put(user_move);
    if (ai_move[0])
        put(ai_move);
    position("POSITION");
    LogText("GEOCHESS MOVED");
}

/* the game saved: its moves kept for a load (and the position) */
static char saved_moves[sizeof moves];

void gc_test_saved(void)
{
    strcpy(saved_moves, moves);
    position("SAVEDPOS");
    LogText("GEOCHESS SAVED");
}

/* a game loaded: it continues from the saved moves */
void gc_test_loaded(void)
{
    strcpy(moves, saved_moves);
    position("LOADED");
    LogText("GEOCHESS LOADED");
}

/* the emulator's cycle counter ($9FB8: a write resets it, a read of
   $9FB8 latches it, then $9FB9-$9FBB): "TIME <what> <cycles, hex>" */
void gc_clock_reset(void)
{
    *(volatile unsigned char *)0x9FB8 = 0;
}

void gc_clock_log(const char *what)
{
    unsigned char c[4], i, n = 5;
    c[0] = *(volatile unsigned char *)0x9FB8;
    c[1] = *(volatile unsigned char *)0x9FB9;
    c[2] = *(volatile unsigned char *)0x9FBA;
    c[3] = *(volatile unsigned char *)0x9FBB;
    memcpy(line, "TIME ", 5);
    while (*what)
        line[n++] = *what++;
    line[n++] = ' ';
    for (i = 4; i-- > 0;) {
        line[n++] = hex[c[i] >> 4];
        line[n++] = hex[c[i] & 15];
    }
    line[n] = 0;
    LogText(line);
}
