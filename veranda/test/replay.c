/*
 * The host engine's answer to a game (the Veranda gates' reference):
 * plays the user's moves through playerMove and lets aiMove answer each,
 * as HandleBoardClick does, then prints the position in the format the
 * test build writes (gc_test.c's POSITION file).
 *
 *   replay e2e4 g1f3 ...       (an illegal move: "illegal <move>", exit 1)
 *   replay -t e2e4 ...         also the clock: think time per AI move
 *
 * Part of the Veranda port (see NOTICE).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "geochess-ai.h"

static char log_[512];

static void put(const char *m)
{
    if (log_[0])
        strcat(log_, " ");
    strcat(log_, m);
}

int main(int argc, char **argv)
{
    int i, r, c, timing = 0;

    engine_init();
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-t") == 0) {
            timing = 1;
            continue;
        }
        if (strlen(argv[i]) != 4) {
            printf("illegal %s\n", argv[i]);
            return 1;
        }
        strcpy(user_move, argv[i]);
        if (!playerMove()) {
            printf("illegal %s\n", argv[i]);
            return 1;
        }
        put(user_move);
        if (engine_state == ENGINE_PLAY) {
            clock_t t0 = clock();
            if (aiMove())
                put(ai_move);
            if (timing)
                fprintf(stderr, "think %s %.3f ms\n", ai_move,
                        (clock() - t0) * 1000.0 / CLOCKS_PER_SEC);
        }
    }
    for (r = 0; r < 8; ++r) {
        for (c = 0; c < 8; ++c)
            printf("%02X%s", board[r * 16 + c], c == 7 ? "\n" : " ");
    }
    printf("side %d\nrights %d\nep %d\nstate %d\nmoves %s\n",
           side, castle_rights, ep_square, engine_state, log_);
    return 0;
}
