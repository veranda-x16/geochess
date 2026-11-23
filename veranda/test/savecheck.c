/*
 * A game saved by geoChess on Veranda, checked with the host's own copy
 * of the saved game code (save_extract.h: the SaveGame record and
 * PackGame, SaveDataOK, RestoreGame cut from the sources by
 * tools/hosttest.sh, as the host suite's savetest uses them).
 *
 *   savecheck <Name.cvt> e2e4 g1f3 ...
 *
 * The moves are the user's; the host engine answers them as on Veranda
 * (replay.c). Checked: the file's data (from byte 508, docs/APPFORMAT.md
 * of Veranda) is one SaveGame record; SaveDataOK accepts it; it is byte
 * for byte the record PackGame makes from the host's game, its move log
 * kept as UpdateNotation keeps it; RestoreGame gives the host's position.
 *
 * Part of the Veranda port (see NOTICE).
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

/* geochess.c's UpdateNotation: the last NOTATION_ROWS plies */
static void note(unsigned char player, const char *text)
{
    unsigned char i;
    if (notation_row_count >= NOTATION_ROWS) {
        for (i = 1; i < NOTATION_ROWS; ++i) {
            strcpy(notation_moves[i - 1], notation_moves[i]);
            notation_players[i - 1] = notation_players[i];
        }
        notation_row_count = NOTATION_ROWS - 1;
    }
    strcpy(notation_moves[notation_row_count], text);
    notation_players[notation_row_count] = player;
    ++notation_row_count;
}

int main(int argc, char **argv)
{
    static unsigned char file[4096];
    static SaveGame want, got;
    unsigned char snap[128];
    int i, n, s, r, e;
    FILE *f;

    if (argc < 2 || !(f = fopen(argv[1], "rb"))) {
        printf("savecheck: cannot open the file\n");
        return 1;
    }
    n = (int)fread(file, 1, sizeof file, f);
    fclose(f);
    if (n != 508 + (int)sizeof(SaveGame)) {
        printf("savecheck: %d bytes of data, want one record of %d\n", n - 508, (int)sizeof(SaveGame));
        return 1;
    }
    memcpy(&got, file + 508, sizeof got);
    if (!SaveDataOK(&got)) {
        printf("savecheck: SaveDataOK refuses the record\n");
        return 1;
    }

    engine_init();
    for (i = 2; i < argc; ++i) {
        strcpy(user_move, argv[i]);
        if (!playerMove()) {
            printf("savecheck: illegal %s\n", argv[i]);
            return 1;
        }
        note(WHT, user_move);
        if (engine_state == ENGINE_PLAY && aiMove())
            note(BLK, ai_move);
    }
    PackGame(&want);
    if (memcmp(&want, &got, sizeof want) != 0) {
        for (i = 0; i < (int)sizeof want; ++i)
            if (((unsigned char *)&want)[i] != ((unsigned char *)&got)[i])
                break;
        printf("savecheck: the record differs from the host's at byte %d\n", i);
        return 1;
    }

    memcpy(snap, board, sizeof snap);
    s = side;
    r = castle_rights;
    e = ep_square;
    engine_init();
    RestoreGame(&got);
    if (memcmp(snap, board, sizeof snap) != 0 || s != side || r != castle_rights || e != ep_square) {
        printf("savecheck: RestoreGame does not give the host's position\n");
        return 1;
    }
    printf("savecheck: %d bytes, one record; SaveDataOK; equal to the host's PackGame; RestoreGame gives the position\n",
           (int)sizeof got);
    return 0;
}
