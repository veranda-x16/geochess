/*
 * The harness of mksavetest.py, for tools/hosttest.sh (which extracts the
 * record and the save code from the sources into save_extract.h).
 *
 * Round trips the saved game format and checks that the validator
 * rejects damaged records before any of it reaches the engine.
 */
#include <stdio.h>
#include <string.h>
#include "geochess-ai.h"

#define NOTATION_ROWS 11
#define WHT 0
#define BLK 1
#define EMPTY 0

static unsigned char notation_row_count = 0;
static char notation_moves[NOTATION_ROWS][5];
static unsigned char notation_players[NOTATION_ROWS];

#include "save_extract.h"


/* ------------------------------------------------------------------ */

static int fails = 0;

static void chk(const char *n, int got, int want)
{
    printf("  %-52s %s\n", n, got == want ? "OK" : "*** FAIL ***");
    if (got != want) { fails++; printf("      got %d want %d\n", got, want); }
}

static SaveGame rec;
static unsigned char board_snap[128];

static int play(const char *from, const char *to)
{
    ChessMove m;
    m.from = ((8 - (from[1] - '0')) * 16) + (from[0] - 'a');
    m.to = ((8 - (to[1] - '0')) * 16) + (to[0] - 'a');
    ChessFormatMove(&m, user_move);
    return playerMove();
}

int main(void)
{
    int i, side_before, rights_before, ep_before, state_before;

    printf("Save record:\n");
    printf("  sizeof(SaveGame) = %d bytes\n", (int)sizeof(SaveGame));

    /* --- round trip of the opening position --------------------- */
    engine_init();
    notation_row_count = 0;
    memset(notation_moves, 0, sizeof(notation_moves));
    memset(notation_players, 0, sizeof(notation_players));

    PackGame(&rec);
    chk("fresh game packs to a valid record", SaveDataOK(&rec), 1);

    memcpy(board_snap, board, sizeof(board_snap));
    side_before = side; rights_before = castle_rights;
    ep_before = ep_square; state_before = engine_state;

    memset(board, 0, sizeof(board));
    side = CBLACK; castle_rights = 0; ep_square = 99; engine_state = 3;

    RestoreGame(&rec);
    for (i = 0; i < 128; ++i)
        if (board[i] != board_snap[i]) break;
    chk("round trip restores every square", i, 128);
    chk("round trip restores side", side, side_before);
    chk("round trip restores castling rights", castle_rights, rights_before);
    chk("round trip restores en passant target", ep_square, ep_before);
    chk("round trip restores engine state", engine_state, state_before);

    /* --- a game with rights spent and an en passant target ------- */
    engine_init();
    play("e2", "e4"); side = CWHITE;      /* keep driving as white */
    play("g1", "f3"); side = CWHITE;
    play("f1", "c4"); side = CWHITE;
    play("e1", "g1"); side = CWHITE;      /* castles: rights change */
    play("d2", "d4"); side = CWHITE;      /* double push: ep target */

    chk("white has castled, rights withdrawn",
        castle_rights & (CR_WK | CR_WQ), 0);
    chk("double push left an en passant target",
        ep_square >= 0, 1);

    notation_row_count = 3;
    strcpy(notation_moves[0], "e2e4"); notation_players[0] = WHT;
    strcpy(notation_moves[1], "e7e5"); notation_players[1] = BLK;
    strcpy(notation_moves[2], "e1g1"); notation_players[2] = WHT;

    PackGame(&rec);
    chk("mid-game record validates", SaveDataOK(&rec), 1);

    memcpy(board_snap, board, sizeof(board_snap));
    ep_before = ep_square; rights_before = castle_rights;

    engine_init();                        /* wipe everything */
    notation_row_count = 0;
    memset(notation_moves, 0, sizeof(notation_moves));

    RestoreGame(&rec);
    for (i = 0; i < 128; ++i)
        if (board[i] != board_snap[i]) break;
    chk("mid-game board restored", i, 128);
    chk("mid-game rights restored", castle_rights, rights_before);
    chk("mid-game ep target restored", ep_square, ep_before);
    chk("move log count restored", notation_row_count, 3);
    chk("move log entry restored", strcmp(notation_moves[2], "e1g1"), 0);
    chk("move log player restored", notation_players[1], BLK);
    chk("restored position is legal", ChessPositionOK(), 1);

    /* --- the validator rejects damaged records ------------------- */
    PackGame(&rec); rec.magic0 = 'X';
    chk("wrong magic rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.version = 99;
    chk("wrong version rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.squares[0] = 200;
    chk("impossible piece code rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.squares[4] = 0;       /* delete the black king */
    chk("missing king rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.squares[20] = CBLACK | 3;   /* a second one */
    chk("duplicate king rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.squares[30] = CWHITE | 2;   /* white black-pawn */
    chk("pawn of the wrong colour rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.mover = 7;
    chk("bad side to move rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.rights = 0xFF;
    chk("stray castling-rights bits rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.ep = 200;
    chk("out of range ep target rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.log_count = 99;
    chk("impossible log length rejected", SaveDataOK(&rec), 0);

    PackGame(&rec); rec.state = 42;
    chk("impossible engine state rejected", SaveDataOK(&rec), 0);

    PackGame(&rec);
    chk("an untouched record still validates", SaveDataOK(&rec), 1);

    /* --- rights cannot be conjured by editing the file ----------- */
    engine_init();
    PackGame(&rec);
    rec.squares[7] = 0;                 /* remove the h8 rook ... */
    rec.rights = CR_ALL;                /* ... but claim all rights */
    chk("record with a missing rook still structurally valid",
        SaveDataOK(&rec), 1);
    RestoreGame(&rec);
    chk("restore withdraws the unsupported right",
        castle_rights & CR_BK, 0);
    chk("the other rights survive", castle_rights & CR_WK, CR_WK);

    printf("\n%d failure(s)\n", fails);
    return fails != 0;
}
