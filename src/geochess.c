/*
 * GEOCHESS
 *
 * Chess for GEOS on the Commodore 64 and 128.
 *
 * Original application:
 *   Scott Hutter, November 2023.
 *
 * You are free to modify this code as desired, as long as original
 * author credit is mentioned for both the GEOS code and the
 * included AI engines.
 *
 * Engine attribution and license are retained in geochess-ai.h.
 *
 * Corrected source revision: GEOCHESS_FIXSET_1.
 *
 * The engine board is authoritative. gboard is only a display cache.
 *
 * Move generation is verified host-side by perft, including castling
 * and en passant. This revision has not been hardware-tested.
 */

#include <stdlib.h>
#include <string.h>

#include "geochess.h"
#include "geochess-ai.h"
#include "squares.h"    /* Veranda port: the squares, from GEOCHESSFONT40.cvt */

#ifdef GC_TEST
/* Veranda port: the test build's hooks (veranda/test/gc_test.c) */
void gc_test_start(void);
void gc_test_moved(void);
void gc_test_saved(void);
void gc_test_loaded(void);
void gc_clock_reset(void);
void gc_clock_log(const char *what);
unsigned char gc_think;
#endif

/*
 * The generated geochess-res.h is intentionally not included.
 * The active menu resources are defined in geochess.h.
 */

static struct menu *current_menu = 0;

static char saved_status[32] = "";

static unsigned char hook_installed = 0;
static unsigned char view_ready = 0;
static unsigned char input_busy = 0;

/* Veranda port, Veranda Look 2: its colours (Look 2's palette, not the
   Classic theme); the Open and Save dialog's RAM bank (overlay 1) */
#define RAMBANK (*(volatile unsigned char *)0)
static unsigned char look = 0;
static unsigned char dlg_bank = 0;
static const struct vrwin panel = {
    PANEL_CX, PANEL_CY, PANEL_CW, PANEL_CH, 0, 2, VW_FRAME | VW_SHADOW
};

static void ConfigureMenus(void)
{
    /* Veranda port: one screen, one menu */
    sc_width = 1;
    current_menu = (struct menu *)&mainMenuV;
}

/* Veranda port: the strings' widths in the UI font (the menus' font):
   an item's text, its $1F not drawn, its key after a TAB 24 pixels
   further (Look 3's menus, geochess.h) */
static unsigned int strw(const char *t)
{
    unsigned int w = 0;
    while (*t) {
        if (*t == '\t')
            w += 24;
        else if (*t != 0x1F)
            w += GetCharWidth(*t);
        ++t;
    }
    return w;
}

static unsigned int widest(const struct gc_item *it, unsigned char n)
{
    unsigned int w = 0, k;
    while (n--) {
        k = strw((it++)->text);
        if (k > w)
            w = k;
    }
    return w + 12;
}

static void DeskAccessory(unsigned char i);
static void Da0(void) { DeskAccessory(0); }
static void Da1(void) { DeskAccessory(1); }
static void Da2(void) { DeskAccessory(2); }
static void Da3(void) { DeskAccessory(3); }
static void Da4(void) { DeskAccessory(4); }
static void Da5(void) { DeskAccessory(5); }

/*
 * Veranda port: the geoChess menu's desk accessories (with the
 * applications, /VERANDA/APPS: Veranda's vrpath.h) between About and
 * Quit; the lists' widths and the bar's end (the kernel takes clicks up
 * to it) in the UI font
 */
static void InitMenus(void)
{
    static void *const das[GC_DA_MAX] = { Da0, Da1, Da2, Da3, Da4, Da5 };
    unsigned char i;

    vr_cd(VR_APPS);
    n_da = (unsigned char)(GC_DA_MAX - FindFTypes(da_names, DESK_ACC, GC_DA_MAX, 0));
    if (n_da > GC_DA_MAX)
        n_da = 0;
    ChDir((char *)curDirName);
    /* the names from 17 bytes apart to DA_TEXT apart, after a byte for
       the group's mark (the last first: they move up) */
    for (i = n_da; i--; ) {
        memmove(da_names + i * DA_TEXT + 1, da_names + i * 17, 17);
        da_names[i * DA_TEXT] = i ? ' ' : 0x1F;
    }
    for (i = 0; i < n_da; ++i) {
        gcMenuV.i[1 + i].text = da_names + i * DA_TEXT + (i ? 1 : 0);
        gcMenuV.i[1 + i].type = MENU_ACTION;
        gcMenuV.i[1 + i].p = das[i];
    }
    gcMenuV.i[1 + n_da].text = "\x1fQuit\tCtrl Q";
    gcMenuV.i[1 + n_da].type = MENU_ACTION;
    gcMenuV.i[1 + n_da].p = QuitGame;
    gcMenuV.flags = (unsigned char)(VERTICAL | (2 + n_da));
    gcMenuV.bottom = (unsigned char)(16 + (2 + n_da) * 16);

    UseFont(FONT_UI);
    gcMenuV.right = widest(gcMenuV.i, 2 + n_da);
    fileMenuV.right = widest(fileMenuV.i, 3);
    optionsMenuV.right = widest(optionsMenuV.i, 1);
    mainMenuV.right = vr_bar_end(bar_names, 3);     /* (ends in the system font) */
}

/* Veranda port: the pointer is a sprite; nothing to hide */
static void HideVDCMouse(void)
{
}

static void DrawSquare(unsigned char row, unsigned char col);

static void ChainMouseHandler(void)
{
    if (old_otherPressVec != 0 &&
        old_otherPressVec != MouseClickHandler)
        old_otherPressVec();
}

/*
 * Clear the selection, repainting the square that carried the marker.
 *
 * The marker is drawn into the bitmap rather than floating above it in
 * a sprite, so it has to be painted out again. Repainting the whole
 * square from the display cache also restores the piece standing on
 * it, and leaves gdrawn agreeing with what is on screen.
 */
static void CancelSelection(void)
{
    unsigned char row = sel_row1;
    unsigned char col = sel_col1;

    sel_row1 = 255;
    sel_col1 = 255;
    tctr = 0;

    if (view_ready && row < 8 && col < 8) {
        HideVDCMouse();
        DrawSquare(row, col);
    }
}

static void SyncBoardFromEngine(void)
{
    static const unsigned char display_types[8] = {
        EMPTY,
        WHT_PAWN,
        WHT_PAWN,
        WHT_KING,
        WHT_KNIGHT,
        WHT_BISHOP,
        WHT_ROOK,
        WHT_QUEEN
    };

    unsigned char row;
    unsigned char col;
    unsigned char display_piece;
    int piece;
    int color;

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            piece = board[(int)row * 16 + col];
            color = piece & 24;
            display_piece = EMPTY;

            if (piece != 0 &&
                (color == CWHITE || color == CBLACK)) {
                display_piece = display_types[piece & 7];

                if (display_piece != EMPTY && color == CBLACK)
                    display_piece += 6;
            }

            gboard[row][col] = display_piece;
        }
    }
}

static void DrawSavedStatus(void)
{
    /* Veranda port: in the move log window's status bar (the UI font) */
    vr_win_status(&panel, saved_status, 0);
}

/*
 * Veranda port: the move log shows the whole game, a row a move (white,
 * then black), the last LOG_ROWS rows of it, numbered when the game's
 * start is known.
 */
static void RedrawNotation(void)
{
    struct window rect;
    unsigned char i;
    unsigned char row;
    unsigned char rows;
    unsigned char first;
    unsigned char number;
    unsigned int y;
    static unsigned char row_start[LOG_MAX];
    static char num[5];

    /* where each row starts: a white move, or a black one alone */
    rows = 0;
    for (i = 0; i < log_count; ++i) {
        if (log_players[i] == 0 || i == 0 || log_players[i - 1] != 0)
            row_start[rows++] = i;
    }
    first = rows > LOG_ROWS ? rows - LOG_ROWS : 0;

    rect.top = PANEL_TOP + 16;
    rect.bot = PANEL_BOT;
    rect.left = PANEL_LEFT + 1;
    rect.right = PANEL_RIGHT - 1;
    DrawStdRect(0, &rect);
    VerticalLine(255, PANEL_TOP + 16, PANEL_BOT, PANEL_LEFT + 104);

    UseSystemFont();
    y = LOG_TOP;
    for (row = first; row < rows; ++row) {
        i = row_start[row];
        if (log_base) {
            number = log_base + row;
            num[0] = number >= 100 ? '0' + number / 100 : ' ';
            num[1] = number >= 10 ? '0' + number / 10 % 10 : ' ';
            num[2] = '0' + number % 10;
            num[3] = '.';
            num[4] = 0;
            PutString(num, y, PANEL_LEFT + 6);
        }
        if (log_players[i] == 0) {
            PutString(log_moves[i], y, PANEL_LEFT + 46);
            ++i;
        }
        if (i < log_count && log_players[i] != 0)
            PutString(log_moves[i], y, PANEL_LEFT + 114);
        y += 11;
    }
}

static void RefreshGameStatus(void)
{
    gameState =
        engine_state == ENGINE_PLAY ? INPROGRESS : STOPPED;

    switch (engine_state) {
        case ENGINE_PLAY:
            if (side == CBLACK)
                UpdateStatus("Thinking...");
            else if (ChessInCheck(CWHITE))
                UpdateStatus("**Check!**");
            else
                UpdateStatus("Your move.");
            break;

        case ENGINE_MATE:
            if (side == CWHITE)
                UpdateStatus("White is mated.");
            else
                UpdateStatus("Black is mated.");
            break;

        case ENGINE_STALEMATE:
            UpdateStatus("Stalemate.");
            break;

        default:
            UpdateStatus("Position error.");
            break;
    }
}

static void BellTick(void);     /* Veranda port: the bell's decay */

int main(void)
{
    char msg[32] =
        "GeoChess v" TOSTRING(VERSION) " fix1";

    osType = 0;                 /* Veranda port: no get_ostype() */

    /*
     * Veranda port: overlay 1, the SDK's Open and Save dialog, mapped
     * while it runs (FileDialog); overlay 2, the SDK's window chrome,
     * mapped for good as appBank (the kernel maps it for its calls); long
     * names (Veranda V82); Veranda Look 2's palette and menu bar (V74,
     * V79)
     */
    if (LoadOverlay(1, 0) == 0 && r0 == 0xA000)
        dlg_bank = r1L;
    if (LoadOverlay(2, 0) != 0 || r0 != 0xA000 || !dlg_bank) {
        LogText("GEOCHESS NO OVERLAY");
        EnterDeskTop();
        return 0;
    }
    appBank = r1L;
    RAMBANK = r1L;
    longNames = LONG_NAMES;
    if (curDirName[0] != '/') {
        strcpy((char *)curDirName, "/");
        ChDir("/");
    }
    theme_load();
    theme_palette();
    look = !theme_none();
    theme_menus();

    LogText("GEOCHESS RUNNING");    /* Veranda port: the emulator's console */
#ifdef GC_TEST
    gc_test_start();            /* the script closes the title box too */
#endif
    gameState = STOPPED;

    ConfigureMenus();
    InitMenus();
    InitScreen();
    DoMenu(current_menu);       /* (the bar under the title box) */

    if (ISGEOS128) {
        if (C128_80_COL_MODE)
            DlgBoxOk(msg, "Commodore 128 80 column mode");
        else
            DlgBoxOk(msg, "Commodore 128 40 column mode");
    } else {
        DlgBoxOk(msg, "For Veranda on the X16");   /* (fits the box in the UI font) */
    }

    if (!LoadFont()) {
        QuitGame();
        return 0;
    }

    view_ready = 1;

    NewGame();

    /*
     * Install the input callback only after fonts, rectangles and
     * engine state have been initialized.
     */
    hook_into_system();
    vr_set_irq(BellTick);

    /*
     * Veranda port: main returns and the kernel's main loop runs the
     * rest (cc65's MainLoop() never returned). QuitGame removes the
     * hook; exit() would go to the shell without it.
     */
    return 0;
}

/*
 * Veranda port: main RAM is full; the drawing and the menus' routines
 * run from overlay 2 (mapped for good as appBank: the kernel maps it for
 * its calls too). Not the interrupt's (the bell), nor FileDialog, which
 * maps overlay 1 while the Open and Save dialog runs.
 */
#pragma code-name (push, "OVERLAY2")

/*
 * Veranda port: the pieces are bitmaps linked into the program
 * (squares.h, made by tools/mkpieces.py from GEOCHESSFONT40.cvt), so
 * there is no font file to open and nothing can fail.
 */
unsigned char LoadFont(void)
{
    UseSystemFont();
    return 1;
}

/*
 * Draw a rectangle specified in 40-column logical coordinates.
 * Unlike the original helper, this does not modify the caller's
 * rectangle in place.
 */
void DrawRect(unsigned char pattern, struct window *square)
{
    struct window scaled;

    scaled.top = square->top;
    scaled.bot = square->bot;
    scaled.left = square->left * sc_width;
    scaled.right = square->right * sc_width;

    DrawStdRect(pattern, &scaled);
}

/*
 * Build the rectangle for one square. The result is shared scratch,
 * so use it before calling this again.
 */
static struct window *SquareRect(unsigned char row, unsigned char col)
{
    sq_rect.top = sq_top[row];
    sq_rect.bot = sq_top[row] + SQUARE_HEIGHT - 1;
    sq_rect.left = sq_left[col];
    sq_rect.right = sq_left[col] + SQUARE_WIDTH - 1;

    return &sq_rect;
}

/* Draw a rectangle already expressed in screen coordinates. */
void DrawStdRect(unsigned char pattern, struct window *square)
{
    SetPattern(pattern);
    vc_rect(square);
}

unsigned char GetPieceChar(unsigned char row, unsigned char col)
{
    static const unsigned char glyphs[2][13] = {
        {
            EMPTY,
            WHT_KING_WHT_SQR,
            WHT_QUEEN_WHT_SQR,
            WHT_BISHOP_WHT_SQR,
            WHT_KNIGHT_WHT_SQR,
            WHT_ROOK_WHT_SQR,
            WHT_PAWN_WHT_SQR,
            BLK_KING_WHT_SQR,
            BLK_QUEEN_WHT_SQR,
            BLK_BISHOP_WHT_SQR,
            BLK_KNIGHT_WHT_SQR,
            BLK_ROOK_WHT_SQR,
            BLK_PAWN_WHT_SQR
        },
        {
            EMPTY,
            WHT_KING_BLK_SQR,
            WHT_QUEEN_BLK_SQR,
            WHT_BISHOP_BLK_SQR,
            WHT_KNIGHT_BLK_SQR,
            WHT_ROOK_BLK_SQR,
            WHT_PAWN_BLK_SQR,
            BLK_KING_BLK_SQR,
            BLK_QUEEN_BLK_SQR,
            BLK_BISHOP_BLK_SQR,
            BLK_KNIGHT_BLK_SQR,
            BLK_ROOK_BLK_SQR,
            BLK_PAWN_BLK_SQR
        }
    };

    unsigned char piece;
    unsigned char color;

    if (row >= 8 || col >= 8)
        return EMPTY;

    piece = gboard[row][col];
    color = SQUARE_COLOR(row, col);

    if (piece > BLK_PAWN || color > BLK)
        return EMPTY;

    /* EMPTY maps explicitly to zero, not an uninitialized local. */
    return glyphs[color][piece];
}

void InitScreen(void)
{
    struct window rect;

    /*
     * Veranda port: the kernel's menu bar is the title (Veranda Look 2:
     * 16 lines and a line under it, the clock at its right); the screen
     * under it cleared, its colours the normal paper
     */
    HideVDCMouse();

    rect.top = 17;
    rect.bot = 479;
    rect.left = 0;
    rect.right = 639;
    DrawStdRect(0, &rect);
    if (look)
        ColorRect(0, 80, 2, 58, 0);
}

/*
 * Veranda port: the board's colours. With the colour map on, every
 * square is drawn as a light one (the pieces on paper) and the cards
 * give the light and the dark squares their colours; off, the dark
 * squares are ink as on GEOS.
 */
static void ColorBoard(void)
{
    unsigned char row;
    unsigned char col;

    /* Veranda Look 2: the frame's ring of cards round the board, chrome */
    if (look) {
        ColorRect(BOARD_LEFT / 8 - 1, 8 * SQUARE_CARDS + 2, BOARD_TOP / 8 - 1, 1, COLOR_FRAME);
        ColorRect(BOARD_LEFT / 8 - 1, 8 * SQUARE_CARDS + 2, BOARD_TOP / 8 + 8 * SQUARE_CARDS, 1, COLOR_FRAME);
        ColorRect(BOARD_LEFT / 8 - 1, 1, BOARD_TOP / 8, 8 * SQUARE_CARDS, COLOR_FRAME);
        ColorRect(BOARD_LEFT / 8 + 8 * SQUARE_CARDS, 1, BOARD_TOP / 8, 8 * SQUARE_CARDS, COLOR_FRAME);
    }

    if (!color_on) {
        if (look)                           /* the squares the normal paper */
            ColorRect(BOARD_LEFT / 8, 8 * SQUARE_CARDS, BOARD_TOP / 8, 8 * SQUARE_CARDS, 0);
        else
            ColorRect(0, 80, 0, 60, 255);   /* the map off (Classic) */
        return;
    }

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            ColorRect(
                BOARD_LEFT / 8 + col * SQUARE_CARDS, SQUARE_CARDS,
                BOARD_TOP / 8 + row * SQUARE_CARDS, SQUARE_CARDS,
                SQUARE_COLOR(row, col) ? COLOR_DARK : COLOR_LIGHT
            );
        }
    }
}

void InitBoard(unsigned char initialPosition)
{
    unsigned char row;
    unsigned char col;

    /*
     * Kept for source-interface compatibility.
     * NewGame() resets the engine; this routine only redraws it.
     */
    (void)initialPosition;

    HideVDCMouse();
    SyncBoardFromEngine();

    /* Veranda port: a frame of two lines; the squares fill the inside */
    WFrameRectangle(255, BOARD_LEFT - 3, BOARD_TOP - 3,
                    BOARD_LEFT + 8 * SQUARE_WIDTH + 2,
                    BOARD_TOP + 8 * SQUARE_HEIGHT + 2);
    WFrameRectangle(255, BOARD_LEFT - 1, BOARD_TOP - 1,
                    BOARD_LEFT + 8 * SQUARE_WIDTH,
                    BOARD_TOP + 8 * SQUARE_HEIGHT);

    /* Rebuild the square geometry for the current mode. */
    for (row = 0; row < 8; ++row)
        sq_top[row] = BOARD_TOP + row * SQUARE_HEIGHT;

    for (col = 0; col < 8; ++col)
        sq_left[col] = BOARD_LEFT + col * SQUARE_WIDTH;

    ColorBoard();

    UseSystemFont();

    for (col = 0; col < 8; ++col) {
        PutChar(
            'a' + col,
            BOARD_TOP + 8 * SQUARE_HEIGHT + 16,
            BOARD_LEFT + SQUARE_WIDTH / 2 - 3 + SQUARE_WIDTH * col
        );
    }

    for (row = 0; row < 8; ++row) {
        PutChar(
            '8' - row,
            BOARD_TOP + SQUARE_HEIGHT / 2 + 3 + SQUARE_HEIGHT * row,
            BOARD_LEFT - 16
        );
    }

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col)
            DrawSquare(row, col);
    }
}

void InitMovePanel(void)
{
    /*
     * Veranda port: the move log is a window of Veranda's chrome (vrwin.h,
     * Look 2): its title band, the log, the status bar (DrawSavedStatus)
     */
    vr_win(&panel, "Moves");
    VerticalLine(255, PANEL_TOP + 16, PANEL_BOT, PANEL_LEFT + 104);
    UseSystemFont();
}

void NewGame(void)
{
    if (input_busy)
        return;

    input_busy = 1;

    CancelSelection();

    notation_row_count = 0;
    log_count = 0;
    log_base = 1;

    memset(notation_moves, 0, sizeof(notation_moves));
    memset(notation_players, 0, sizeof(notation_players));
    memset(gboard, 0, sizeof(gboard));
    memset(gdrawn, 0, sizeof(gdrawn));

    /*
     * Reset every engine square before rebuilding the display.
     * No pieces or selection state survive from the old game.
     */
    engine_init();

    InitBoard(1);
    InitMovePanel();
    RefreshGameStatus();

    input_busy = 0;

    DoMenu(current_menu);
}

void UpdateNotation(
    unsigned char player,
    unsigned char src_row,
    unsigned char src_col,
    unsigned char dest_row,
    unsigned char dest_col
)
{
    ChessMove move;
    char text[5];
    unsigned char i;

    if (player > 1 ||
        src_row >= 8 ||
        src_col >= 8 ||
        dest_row >= 8 ||
        dest_col >= 8)
        return;

    move.from = (int)src_row * 16 + src_col;
    move.to = (int)dest_row * 16 + dest_col;

    /*
     * Every valid destination, including row zero/rank eight,
     * produces a complete four-character coordinate move.
     */
    ChessFormatMove(&move, text);

    /*
     * Scroll the oldest entry off the top instead of clearing the
     * log, so the panel always shows the most recent moves.
     */
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

    /* Veranda port: the panel's log; full, a move is dropped */
    if (log_count >= LOG_MAX) {
        memmove(log_moves, log_moves[2], sizeof(log_moves) - 2 * 5);
        memmove(log_players, log_players + 2, LOG_MAX - 2);
        log_count -= 2;
        if (log_base)
            ++log_base;
    }
    strcpy(log_moves[log_count], text);
    log_players[log_count] = player;
    ++log_count;

    RedrawNotation();
}

void UpdateStatus(const char *message)
{
    if (message == 0)
        message = "";

    if (message != saved_status) {
        strncpy(
            saved_status,
            message,
            sizeof(saved_status) - 1
        );

        saved_status[sizeof(saved_status) - 1] = '\0';
    }

    DrawSavedStatus();
}

/*
 * Repaint one square from the display cache.
 * The chess character set must already be loaded.
 */
static void DrawSquare(unsigned char row, unsigned char col)
{
    /*
     * Veranda port: one bitmap a square, the piece on its fill. With the
     * colour map every square is drawn light (the cards colour it).
     */
    WBitmapUp(
        (void *)square_bits[color_on ? 0 : SQUARE_COLOR(row, col)]
                           [gboard[row][col]],
        sq_left[col] / 8, SQUARE_CARDS,
        sq_top[row], SQUARE_HEIGHT
    );

    gdrawn[row][col] = gboard[row][col];
}

/*
 * Mark the selected square.
 *
 * This used to be a GEOS sprite, but a sprite bitmap is a fixed three
 * bytes across. That is 24 pixels, which suits a 40 column square but
 * covers barely half of the 36 pixel wide square in 80 column mode,
 * where the horizontal resolution doubles and the vertical does not.
 * Drawing the marker instead scales with the board, because the
 * rectangle it frames is the same one the square was drawn from.
 *
 * The border is inverted rather than drawn solid. Squares alternate
 * between a light and a dark fill pattern, and the font carries a
 * separate glyph for a piece on each, so a frame drawn in set pixels
 * would stand out on a light square and vanish into a dark one.
 * Inverting guarantees contrast against whatever is underneath.
 *
 * Inverting the outer rectangle and then the inner one flips the
 * border twice over the interior, which leaves the square and the
 * piece standing on it exactly as they were and marks only the edge.
 */
static void DrawSelection(unsigned char row, unsigned char col)
{
    HideVDCMouse();

    vc_invert(SquareRect(row, col));

    /*
     * A two pixel border, matching the sprite this replaced. Rows are
     * the same height in both modes so the vertical inset is a flat
     * two, but 80 column pixels are half as wide, so the horizontal
     * inset scales to keep the border the same physical thickness.
     */
    /* Veranda port: three pixels each way on the 48 pixel square */
    sq_rect.top += 3;
    sq_rect.bot -= 3;
    sq_rect.left += 3;
    sq_rect.right -= 3;

    vc_invert(&sq_rect);
}

/*
 * The engine has already committed the move.
 *
 * Repaint whichever squares differ from what is on screen rather than
 * assuming a move touches only its origin and destination: castling
 * moves a rook as well, and en passant empties a square the capturing
 * pawn never lands on.
 */
void RefreshBoardDisplay(void)
{
    unsigned char row;
    unsigned char col;

    HideVDCMouse();
    SyncBoardFromEngine();

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col) {
            if (gboard[row][col] != gdrawn[row][col])
                DrawSquare(row, col);
        }
    }
}

/*
 * Notification bell.
 *
 * Veranda port: the SID is now VERA's PSG (voice 0 of 16, its registers
 * at VRAM $1F9C0: frequency low and high, right/left and volume,
 * waveform and pulse width). The PSG has no envelope, so the decay the
 * SID did by itself is a routine in the interrupt (vr_set_irq) that
 * lowers the volume a step every frame until the note is silent.
 *
 * The routine also starts the note: PlayBell only sets bell_start, so
 * VERA's address registers are touched in the interrupt alone, which
 * saves and restores them around its work (Veranda DECISIONS V8, V10).
 * Nothing else here or in the kernel uses the PSG.
 *
 * The pitch is the original's: about 1760 Hz, a triangle wave. The
 * PSG's frequency word is f * 2^17 / 48828.125, so 4724 ($1274).
 */
#define BELL_FREQ          0x1274      /* about 1760 Hz */
#define BELL_VOLUME        63          /* the most */
#define BELL_LR            0xC0        /* both channels */
#define BELL_TRIANGLE      0x80        /* waveform 2 */

#define VERA_ADDR_L        (*(volatile unsigned char *)0x9F20)
#define VERA_ADDR_M        (*(volatile unsigned char *)0x9F21)
#define VERA_ADDR_H        (*(volatile unsigned char *)0x9F22)
#define VERA_DATA0         (*(volatile unsigned char *)0x9F23)
#define VERA_CTRL          (*(volatile unsigned char *)0x9F25)

static volatile unsigned char bell_start = 0;
static volatile unsigned char bell_level = 0;

#pragma code-name (pop)

/* point port 0 at voice 0's register reg, stepping by one */
static void BellSeek(unsigned char reg)
{
    VERA_CTRL = 0;
    VERA_ADDR_L = 0xC0 + reg;
    VERA_ADDR_M = 0xF9;
    VERA_ADDR_H = 0x11;
}

/* every frame, in the interrupt */
static void BellTick(void)
{
    unsigned char v;

    if (bell_start) {
        bell_start = 0;
        bell_level = BELL_VOLUME;
        BellSeek(0);
        VERA_DATA0 = BELL_FREQ & 0xFF;
        VERA_DATA0 = BELL_FREQ >> 8;
        VERA_DATA0 = BELL_LR | BELL_VOLUME;
        VERA_DATA0 = BELL_TRIANGLE;
        return;
    }

    v = bell_level;
    if (v == 0)
        return;

    /* about 3/4 of a second down to silence, as the SID's decay */
    v -= (v >> 4) + 1;
    bell_level = v;
    BellSeek(2);
    VERA_DATA0 = BELL_LR | v;
}

static void PlayBell(void)
{
#ifdef GC_TEST
    LogText("GEOCHESS BELL");
#endif
    bell_start = 1;
}

/*
 * Leave the PSG quiet for whatever runs after us. This one write is made
 * outside the interrupt, so VERA's port 0 is put back as it was, with
 * interrupts off while it is borrowed.
 */
static void SilenceBell(void)
{
    unsigned char ctrl, l, m, h;

    vr_set_irq(0);

    __asm__("php");
    __asm__("sei");
    ctrl = VERA_CTRL;
    VERA_CTRL = ctrl & 0x7E;            /* port 0, no reset */
    l = VERA_ADDR_L;
    m = VERA_ADDR_M;
    h = VERA_ADDR_H;
    BellSeek(2);
    VERA_DATA0 = 0;
    VERA_CTRL = ctrl & 0x7E;
    VERA_ADDR_L = l;
    VERA_ADDR_M = m;
    VERA_ADDR_H = h;
    VERA_CTRL = ctrl;
    __asm__("plp");
}

static void HandleBoardClick(
    unsigned char row,
    unsigned char col
)
{
    unsigned char from_row;
    unsigned char from_col;
    unsigned char to_row;
    unsigned char to_col;
    ChessMove move;

    /*
     * Veranda port: the original skipped two events after a selection
     * here, for the GEOS input drivers' extra calls. Veranda calls
     * otherPressVec once for a press and once for a release, and
     * MouseClickHandler passes on presses only, so every click counts.
     */

    if (sel_row1 == 255) {
        if (gboard[row][col] >= WHT_KING &&
            gboard[row][col] <= WHT_PAWN) {

            DrawSelection(row, col);

            sel_row1 = row;
            sel_col1 = col;
            tctr = 1;
        } else {
            CancelSelection();
        }

        return;
    }

    /* Clicking the selected square again cancels the selection. */
    if (sel_row1 == row && sel_col1 == col) {
        CancelSelection();
        RefreshGameStatus();
        return;
    }

    from_row = sel_row1;
    from_col = sel_col1;

    move.from = (int)from_row * 16 + from_col;
    move.to = (int)row * 16 + col;

    ChessFormatMove(&move, user_move);

    input_busy = 1;
#ifdef GC_TEST
    gc_think = 1;       /* the script waits for the move and the answer */
#endif

    /*
     * Validate and commit in the engine before changing the display
     * or adding an entry to the move log.
     */
    if (!playerMove()) {
        CancelSelection();

        if (engine_state == ENGINE_INVALID)
            RefreshGameStatus();
        else
            UpdateStatus("Illegal move.");

        /*
         * A persistent message replaces the CPU-speed-dependent
         * enable/disable loop previously used for error flashing.
         */
        input_busy = 0;
#ifdef GC_TEST
        gc_think = 0;
#endif
        return;
    }

    CancelSelection();

    RefreshBoardDisplay();
    UpdateNotation(0, from_row, from_col, row, col);

    RefreshGameStatus();

    if (engine_state == ENGINE_PLAY) {
        UpdateStatus("Thinking...");

#ifdef GC_TEST
        gc_clock_reset();
#endif
        if (aiMove()) {
#ifdef GC_TEST
            gc_clock_log(ai_move);      /* the engine's think time */
#endif
            from_col = xlateCol(ai_move[0]);
            from_row = xlateRow(ai_move[1]);
            to_col = xlateCol(ai_move[2]);
            to_row = xlateRow(ai_move[3]);

            /*
             * Display a committed AI move even when that move has
             * just ended the game.
             */
            RefreshBoardDisplay();

            UpdateNotation(
                1,
                from_row,
                from_col,
                to_row,
                to_col
            );
        }

        RefreshGameStatus();

        /*
         * Ring once the engine has finished and the move is actually
         * back with the player. A move that ended the game is not a
         * turn, so it stays silent.
         */
#ifdef GC_TEST
        gc_test_moved();
#endif
        if (engine_state == ENGINE_PLAY && side == CWHITE)
            PlayBell();
    }

    input_busy = 0;
#ifdef GC_TEST
    gc_think = 0;
#endif
}

void MouseClickHandler(void)
{
    unsigned char row;
    unsigned char col;

    /* Veranda port: otherPressVec sees releases too (mouseData bit 7) */
    if (mouseData & 0x80) {
        ChainMouseHandler();
        return;
    }

    if (!input_busy &&
        gameState == INPROGRESS &&
        side == CWHITE) {

        for (row = 0; row < 8; ++row) {
            for (col = 0; col < 8; ++col) {
                if (vc_inside(SquareRect(row, col)) == 255) {
                    HandleBoardClick(row, col);
                    ChainMouseHandler();
                    return;
                }
            }
        }
    }

    ChainMouseHandler();
}

/*
 * Veranda port: the Open and Save dialog (overlay 1, its bank mapped
 * while it runs): the folder's .cvt files, the name into save_name
 * without its ".cvt"; 1 when one was given
 */
static const char gc_types[] = "Saved games:CVT";

static unsigned char FileDialog(unsigned char mode, const char *title)
{
    unsigned char r;
    unsigned char n;

    vr_file_types = gc_types;
    vr_file_max = SAVE_NAME_MAX;
    RAMBANK = dlg_bank;
    r = vr_file(mode, title, save_name);
    RAMBANK = appBank;
    if (!r)
        return 0;
    n = (unsigned char)strlen(save_name);
    if (n > 4 && save_name[n - 4] == '.' &&
        (save_name[n - 3] | 0x20) == 'c' &&
        (save_name[n - 2] | 0x20) == 'v' &&
        (save_name[n - 1] | 0x20) == 't')
        save_name[n - 4] = '\0';
    if (save_name[0] == '\0') {
        DlgBoxOk("No name was entered.", "Nothing saved.");
        return 0;
    }
    return 1;
}

#pragma code-name (push, "OVERLAY2")    /* (as above) */

/* Veranda port: geoChess > About geoChess (the title box) */
void AboutMenuHandler(void)
{
    if (input_busy)
        return;

    input_busy = 1;
    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */
    DlgBoxOk("GeoChess v" TOSTRING(VERSION) " fix1",
             "For Veranda on the X16");   /* (fits the box in the UI font) */
    input_busy = 0;
    DoMenu(current_menu);
}

/*
 * Veranda port: a desk accessory from the geoChess menu (Veranda puts the
 * game aside whole and gives it back as it was); the screen drawn again
 * after it, the menu bar's look set again
 */
static void DeskAccessory(unsigned char i)
{
#ifdef GC_TEST
    unsigned int src;
#endif
    if (input_busy)
        return;
#ifdef GC_TEST
    /*
     * the session's script is mouseSource, which an accessory's start
     * and end clear: kept, and its frames stand still meanwhile (the
     * zero page, the player's place in it, comes back with the game); the
     * accessory gets a 'q' (Veranda's calculator closes)
     */
    src = mouseSource;
    gc_think = 1;
    asm("lda #$71");
    asm("jsr $FEC3");           /* the ROM's kbdbuf_put */
#endif
    input_busy = 1;
    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */
    CancelSelection();
    vr_cd(VR_APPS);
    GetFile(da_names + i * DA_TEXT + 1, 0, 0, 0, 0);
    ChDir((char *)curDirName);
#ifdef GC_TEST
    mouseSource = src;
    gc_think = 0;
#endif
    theme_menus();
    InitScreen();
    InitBoard(1);
    InitMovePanel();
    RedrawNotation();
    DrawSavedStatus();
    input_busy = 0;
    DoMenu(current_menu);
}

/*
 * Saved games
 *
 * The record in geochess.h is written to disk verbatim, so packing
 * and unpacking are the whole file format. Only the 64 playable
 * squares are stored; the engine rebuilds nothing else from them.
 */

#define SAVE_PACK(sq)    ((unsigned char)((((sq) >> 4) * 8) + ((sq) & 7)))
#define SAVE_UNPACK(i)   ((int)(((i) / 8) * 16 + ((i) % 8)))

static void PackGame(SaveGame *rec)
{
    unsigned char row;
    unsigned char col;
    unsigned char i;

    memset(rec, 0, sizeof(SaveGame));

    rec->magic0 = SAVE_MAGIC0;
    rec->magic1 = SAVE_MAGIC1;
    rec->version = SAVE_VERSION;

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col)
            rec->squares[row * 8 + col] =
                (unsigned char)board[(int)row * 16 + col];
    }

    rec->mover = side == CBLACK ? BLK : WHT;
    rec->rights = (unsigned char)castle_rights;
    rec->ep = ep_square < 0 ?
        SAVE_NO_EP : SAVE_PACK(ep_square);
    rec->state = engine_state;

    rec->log_count = notation_row_count;

    for (i = 0; i < NOTATION_ROWS; ++i) {
        rec->log_players[i] = notation_players[i];
        strcpy(rec->log_moves[i], notation_moves[i]);
    }
}

/*
 * Validate the packed record before any of it reaches the engine.
 *
 * This repeats the structural rules ChessPositionOK() enforces, but
 * on the file data, so a damaged or foreign file is refused while the
 * game in progress is still intact.
 */
static unsigned char SaveDataOK(const SaveGame *rec)
{
    unsigned char i;
    unsigned char piece;
    unsigned char color;
    unsigned char type;
    unsigned char white_kings = 0;
    unsigned char black_kings = 0;

    if (rec->magic0 != SAVE_MAGIC0 ||
        rec->magic1 != SAVE_MAGIC1 ||
        rec->version != SAVE_VERSION)
        return 0;

    if (rec->mover > BLK ||
        rec->rights > CR_ALL ||
        rec->state > ENGINE_INVALID ||
        rec->log_count > NOTATION_ROWS)
        return 0;

    if (rec->ep != SAVE_NO_EP && rec->ep >= 64)
        return 0;

    for (i = 0; i < 64; ++i) {
        piece = rec->squares[i];

        if (piece == 0)
            continue;

        color = piece & 24;
        type = piece & 7;

        if ((piece & ~31) != 0 ||
            type == 0 ||
            (color != CWHITE && color != CBLACK))
            return 0;

        if ((type == 1 && color != CWHITE) ||
            (type == 2 && color != CBLACK))
            return 0;

        if (type == 3) {
            if (color == CWHITE)
                ++white_kings;
            else
                ++black_kings;
        }
    }

    return white_kings == 1 && black_kings == 1;
}

static void UnpackGame(const SaveGame *rec)
{
    unsigned char row;
    unsigned char col;
    unsigned char i;

    /*
     * Start from the initial layout so the columns outside the board
     * keep exactly what a freshly begun game has in them. Nothing
     * reads them, but a restored game should not be a different shape
     * from one that was just started. The loop below then overwrites
     * every playable square, so no piece survives from before.
     */
    memcpy(board, starting_board, sizeof(board));

    for (row = 0; row < 8; ++row) {
        for (col = 0; col < 8; ++col)
            board[(int)row * 16 + col] = rec->squares[row * 8 + col];
    }

    side = rec->mover == BLK ? CBLACK : CWHITE;
    castle_rights = rec->rights;
    ep_square = rec->ep == SAVE_NO_EP ?
        -1 : SAVE_UNPACK(rec->ep);
    engine_state = rec->state;

    best_src = -1;
    best_dst = -1;

    notation_row_count = rec->log_count;

    for (i = 0; i < NOTATION_ROWS; ++i) {
        notation_players[i] = rec->log_players[i];
        strcpy(notation_moves[i], rec->log_moves[i]);
    }
}

/*
 * Rights recorded in the file are trusted only so far: withdraw any
 * that the restored placement cannot support, so a hand-edited file
 * cannot conjure a castling move out of an empty corner.
 *
 * SaveDataOK() has already passed by this point, so the placement is
 * known to be structurally sound and nothing here can fail.
 */
static void RestoreGame(const SaveGame *rec)
{
    UnpackGame(rec);
    ChessUpdateRights();
    engine_state = ChessStatus(side);
}

void LoadGameMenuHandler(void)
{
    if (input_busy)
        return;

    input_busy = 1;
    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */

    /*
     * Veranda port: the standard Open dialog (Veranda's vrchoose.h, V79),
     * the games by their long names (V82); a CVT file chosen is a saved
     * game if its GEOS type and class are (vc_read_data)
     */
    if (!FileDialog(VRF_OPEN, "Open a saved game")) {
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    if (FindFile(save_name) != 0) {
        DlgBoxOk("Cannot find that file.", save_name);
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    /* Veranda port: no block chains; the data from the CVT file */
    switch (vc_read_data(save_name, SAVE_CLASS, &save_buf, sizeof(save_buf))) {
    case 0:
        break;
    case VC_NOT_GAME:
        DlgBoxOk("That is not a", "GeoChess saved game.");
        input_busy = 0;
        DoMenu(current_menu);
        return;
    default:
        DlgBoxOk("Cannot read that file.", save_name);
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    /*
     * Validate the record while it is still only file data. Nothing
     * has been handed to the engine yet, so a bad file leaves the
     * game in progress exactly as it was and there is nothing to
     * undo.
     */
    if (!SaveDataOK(&save_buf)) {
        DlgBoxOk("That is not a valid", "GeoChess saved game.");
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

#ifdef GC_TEST
    gc_think = 1;               /* the script waits for the redraw */
#endif
    CancelSelection();
    RestoreGame(&save_buf);

    /*
     * Veranda port: the panel's log starts again from the saved eleven
     * plies; where they fall in the game is not saved, so no numbers.
     */
    log_count = notation_row_count;
    memcpy(log_moves, notation_moves, sizeof(notation_moves));
    memcpy(log_players, notation_players, sizeof(notation_players));
    log_base = 0;

#ifdef GC_TEST
    gc_test_loaded();
#endif

    InitBoard(1);
    InitMovePanel();
    RedrawNotation();
    RefreshGameStatus();

    input_busy = 0;

    /*
     * Menus only open on the player's turn, so a restored game hands
     * the move straight back. Announce it the same way the engine
     * does when it finishes thinking.
     */
    if (engine_state == ENGINE_PLAY && side == CWHITE)
        PlayBell();

#ifdef GC_TEST
    gc_think = 0;
#endif
    DoMenu(current_menu);
}

/*
 * A plain document icon for the deskTop: three bytes across by
 * twenty-one rows, which is the 63 bytes a GEOS file header carries.
 */
static void BuildSaveIcon(struct vc_fileheader *hdr)
{
    unsigned char row;
    unsigned char *pic;

    /* Veranda port: width and height, then $BF and the 63 bytes */
    hdr->icon_w = 3;
    hdr->icon_h = 21;
    hdr->icon_pic[0] = 0xBF;
    pic = hdr->icon_pic + 1;

    for (row = 0; row < 21; ++row) {
        if (row == 0 || row == 20) {
            pic[row * 3] = 0xFF;
            pic[row * 3 + 1] = 0xFF;
            pic[row * 3 + 2] = 0xFF;
        } else {
            pic[row * 3] = 0x80;
            pic[row * 3 + 1] = 0x00;
            pic[row * 3 + 2] = 0x01;
        }
    }
}

void SaveGameMenuHandler(void)
{
    /*
     * The header is a local rather than another static: it is only
     * live for the SaveFile() call, and the C stack is already
     * reserved whether or not this uses it.
     */
    static struct vc_fileheader hdr;    /* Veranda port: 256 bytes */

    if (input_busy)
        return;

    input_busy = 1;
    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */

    /*
     * Veranda port: the standard Save dialog (Veranda's vrchoose.h, V79):
     * a long name (V82), in the folder chosen there; the name given
     * last offered again
     */
    if (!FileDialog(VRF_SAVE, "Save this game as")) {
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    if (save_name[0] == '\0') {
        DlgBoxOk("No name was entered.", "Nothing saved.");
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    /* FindFile returns zero when the name is already taken */
    if (FindFile(save_name) == 0) {
        DlgBoxOk("A file of that name", "already exists.");
        input_busy = 0;
        DoMenu(current_menu);
        return;
    }

    PackGame(&save_buf);

    memset(&hdr, 0, sizeof(hdr));
    BuildSaveIcon(&hdr);

    /*
     * SaveFile() takes the file's name in the info block's first two
     * bytes (Veranda as cc65's GEOS bindings). The name must stay put
     * until the call returns, which is why save_name is static.
     */
    hdr.name = save_name;

    hdr.dostype = (char)(0x80 | USR);
    hdr.type = APPL_DATA;
    hdr.structure = SEQUENTIAL;

    hdr.load_address = (unsigned)&save_buf;
    hdr.end_address = (unsigned)&save_buf + sizeof(save_buf);
    hdr.exec_address = (unsigned)&save_buf;

    strcpy(hdr.class_name, SAVE_CLASS);
    strcpy(hdr.author, "GeoChess");

    if (SaveFile(&hdr, 0) != 0)
        DlgBoxOk("Could not save the game.", save_name);
    else {
#ifdef GC_TEST
        gc_test_saved();
#endif
        DlgBoxOk("Game saved as", save_name);
    }

    DrawSavedStatus();

    input_busy = 0;

    DoMenu(current_menu);
}

void NewGameMenuHandler(void)
{
    if (input_busy)
        return;

    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */
    NewGame();
}

void Switch4080MenuHandler(void)
{
    if (input_busy)
        return;

    input_busy = 1;

    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */
    CancelSelection();

    if (ISGEOS128) {
        SetNewMode();

        /* Configure from the resulting mode, not a predicted mode. */
        ConfigureMenus();

        if (!LoadFont()) {
            input_busy = 0;
            QuitGame();
            return;
        }

        InitScreen();
        InitBoard(1);
        InitMovePanel();

        /*
         * Redraw the current notation page and preserve the status.
         * A mode switch must not reset the game or replace mate,
         * stalemate, check or error text with "Your move."
         */
        RedrawNotation();
        DrawSavedStatus();
    }

    input_busy = 0;

    DoMenu(current_menu);
}

/*
 * Veranda port: options/colour board turns the colour map on and off
 * (the squares' paper colours; Veranda DECISIONS V30-V33).
 */
void ColorMenuHandler(void)
{
    if (input_busy)
        return;

    input_busy = 1;
    GotoFirstMenu();            /* Veranda port: the bar stays (Look 2), the item open closed */
    CancelSelection();

    color_on ^= 1;
    optionsMenuV.i[0].text = color_on ? color_on_text : color_off_text;
    InitBoard(1);

    input_busy = 0;
    DoMenu(current_menu);
}

void hook_into_system(void)
{
    if (!hook_installed) {
        /* Veranda port: the vector is a word variable */
        old_otherPressVec = (void_func)otherPressVec;
        otherPressVec = VR_FN(MouseClickHandler);
        keyVector = VR_FN(MenuKey);     /* Veranda port: the menus' keys */
        hook_installed = 1;
    }
}

void remove_hook(void)
{
    if (hook_installed) {
        /*
         * Do not overwrite a different callback installed after ours.
         */
        if (otherPressVec == VR_FN(MouseClickHandler))
            otherPressVec = VR_FN(old_otherPressVec);
        keyVector = 0;

        hook_installed = 0;
    }

    /* Menu resources are static storage. Never pass them to free(). */
}

void QuitGame(void)
{
    input_busy = 1;
    gameState = STOPPED;

    /*
     * The menu's desktop exit may bypass the C exit machinery.
     * Restore our callback explicitly rather than relying only on
     * the atexit handler.
     */
    remove_hook();
    CancelSelection();
    SilenceBell();

    UseSystemFont();

    view_ready = 0;

    EnterDeskTop();
}

/*
 * Veranda port: the menus' keys, as the menus show them (geochess.h):
 * Ctrl N a new game, Ctrl O open, Ctrl S save, Ctrl Q quit (keyData is
 * the control code, keyDataMods bit 1 Ctrl)
 */
void MenuKey(void)
{
    if (input_busy || !(keyDataMods & 2))
        return;
    switch (keyData) {
    case 14: NewGameMenuHandler(); break;
    case 15: LoadGameMenuHandler(); break;
    case 19: SaveGameMenuHandler(); break;
    case 17: QuitGame(); break;
    }
}

#pragma code-name (pop)
