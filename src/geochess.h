/*
 * GEOCHESS
 *
 * Chess for GEOS on the Commodore 64 and 128.
 *
 * Original application:
 *   Scott Hutter, November 2023.
 *
 * Original permission:
 *   You are free to modify this code as desired, as long as original
 *   author credit is mentioned for both the GEOS code and the
 *   included AI engines.
 *
 * Corrected source revision: GEOCHESS_FIXSET_1.
 *
 * This header contains application storage and cc65 GEOS menu
 * resources. Include it in geochess.c only.
 */

#ifndef GEOCHESS_H
#define GEOCHESS_H

#include "vcompat.h"        /* Veranda port: was <geos.h> */

#define VERSION 1.3
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* Veranda port: neither a GEOS 64 nor a GEOS 128 */
#define ISGEOS64           0
#define ISGEOS128          0

#define C128_40_COL_MODE    1
#define C128_80_COL_MODE    0

/*
 * Veranda port: the 640x480 layout. The squares are 48 x 48 pixels, six
 * cards each way, from a card boundary, so a square is one bitmap
 * (WBitmapUp) and one block of the colour map; the move log and the
 * status are to the right of the board.
 */
#define BOARD_TOP          48
#define BOARD_LEFT         48
#define SQUARE_WIDTH       48
#define SQUARE_HEIGHT      48
#define SQUARE_CARDS       6

/*
 * Veranda port, Veranda Look 2 (Veranda's DECISIONS V74, V77, V79): the
 * move log is a window of the SDK's chrome (vrwin.h) on whole cards:
 * cards 58-78 by card rows 6-55 (464-631 x 48-447), its title band (rows
 * 6-7, the theme's accent), the log (from row 8), its status bar (rows
 * 54-55, the chrome: the game's status), a frame and a shadow.
 */
#define PANEL_LEFT         464
#define PANEL_RIGHT        631
#define PANEL_TOP          48
#define PANEL_BOT          431         /* the log's last line */
#define PANEL_CX           58          /* the window in cards */
#define PANEL_CY           6
#define PANEL_CW           21
#define PANEL_CH           50
#define LOG_TOP            74          /* the first row's baseline */
#define LOG_ROWS           33          /* rows of 11 pixels */
#define LOG_MAX            160         /* plies kept for the panel */

/* the colour map's paper for the squares (the theme's palette, theme.h;
   Veranda Look 3 "Sunroom"): the dark squares sage (SAGE, the Sunroom
   desktop's), the light ones mist (SIDEBAR), or in dark mode green (GREEN:
   mist is darker than sage there, green lighter); the board's frame on a
   ring of chrome cards */
#define COLOR_LIGHT        (theme.dark ? GREEN : SIDEBAR)
#define COLOR_DARK         SAGE
#define COLOR_FRAME        CHROME

/* the saved game's log (the file format): the last eleven plies */
#define NOTATION_ROWS      11

#define EMPTY              0

#define WHT_KING           1
#define WHT_QUEEN          2
#define WHT_BISHOP         3
#define WHT_KNIGHT         4
#define WHT_ROOK           5
#define WHT_PAWN           6

#define BLK_KING           7
#define BLK_QUEEN          8
#define BLK_BISHOP         9
#define BLK_KNIGHT         10
#define BLK_ROOK           11
#define BLK_PAWN           12

#define WHT                0
#define BLK                1

#define WHT_KING_WHT_SQR    'A'
#define WHT_QUEEN_WHT_SQR   'B'
#define WHT_BISHOP_WHT_SQR  'C'
#define WHT_KNIGHT_WHT_SQR  'D'
#define WHT_ROOK_WHT_SQR    'E'
#define WHT_PAWN_WHT_SQR    'F'

#define WHT_KING_BLK_SQR    'S'
#define WHT_QUEEN_BLK_SQR   'T'
#define WHT_BISHOP_BLK_SQR  'U'
#define WHT_KNIGHT_BLK_SQR  'V'
#define WHT_ROOK_BLK_SQR    'W'
#define WHT_PAWN_BLK_SQR    'X'

#define BLK_KING_WHT_SQR    'M'
#define BLK_QUEEN_WHT_SQR   'N'
#define BLK_BISHOP_WHT_SQR  'O'
#define BLK_KNIGHT_WHT_SQR  'P'
#define BLK_ROOK_WHT_SQR    'Q'
#define BLK_PAWN_WHT_SQR    'R'

#define BLK_KING_BLK_SQR    'G'
#define BLK_QUEEN_BLK_SQR   'H'
#define BLK_BISHOP_BLK_SQR  'I'
#define BLK_KNIGHT_BLK_SQR  'J'
#define BLK_ROOK_BLK_SQR    'K'
#define BLK_PAWN_BLK_SQR    'L'

enum GameStates {
    INPROGRESS,
    STOPPED
};

/*
 * Saved game record.
 *
 * Written to disk verbatim, so the layout is the file format. The
 * board is packed to one byte per playable square: the engine keeps
 * an int per 0x88 entry, but no piece code exceeds 23 and the
 * off-board columns hold no game state.
 *
 * A magic and a version go first so a file from another program, or
 * from a later revision, is rejected before anything is disturbed.
 */
#define SAVE_MAGIC0        'G'
#define SAVE_MAGIC1        'C'
#define SAVE_VERSION       1

#define SAVE_NO_EP         255

#define SAVE_CLASS         "GeoChess Game"
#define SAVE_CLASS_PREFIX  "GeoChess"

/* Veranda port: a long name (Veranda V82: longNames), the FAT name's
   ".cvt" within the Open and Save dialog's VRF_NAME */
#define SAVE_NAME_MAX      (VRF_NAME - 4)

typedef struct {
    unsigned char magic0;
    unsigned char magic1;
    unsigned char version;
    unsigned char squares[64];
    unsigned char mover;             /* WHT or BLK */
    unsigned char rights;            /* castling rights bits */
    unsigned char ep;                /* packed square, or SAVE_NO_EP */
    unsigned char state;             /* engine_state */
    unsigned char log_count;
    unsigned char log_players[NOTATION_ROWS];
    char log_moves[NOTATION_ROWS][5];
    unsigned char reserved[2];
} SaveGame;

static unsigned char osType = 0;
static unsigned char sc_width = 1;

/*
 * Square geometry, already scaled for the current mode.
 *
 * Top and bottom depend only on the rank and left and right only on
 * the file, so two small tables and one scratch rectangle replace the
 * sixty-four windows this used to keep.
 */
static unsigned int sq_top[8];
static unsigned int sq_left[8];
static struct window sq_rect;

/*
 * Rendering cache: the display piece code per square.
 *
 * Square colour is not stored. It is (row + column) & 1 and never
 * changes, so computing it costs less than keeping a second plane.
 *
 * The engine's board[] is the authoritative chess position.
 */
static unsigned char gboard[8][8];

#define SQUARE_COLOR(row, col)   (((row) + (col)) & 1)

/*
 * What is currently on screen. Comparing it against gboard finds the
 * squares a move actually changed, which is two for an ordinary move,
 * three for en passant and four for castling.
 */
static unsigned char gdrawn[8][8];


/*
 * save_buf holds the record being written or the one just read. It is
 * fully validated before any of it reaches the engine, so a bad file
 * never disturbs the game in progress and nothing has to be rolled
 * back. The 256 byte file header is a local in the save handler
 * rather than another static.
 */
static SaveGame save_buf;
static char save_name[VRF_NAME + 1];   /* (vr_file's 64 bytes) */

static unsigned char sel_row1 = 255;
static unsigned char sel_col1 = 255;
static unsigned char tctr = 0;

static unsigned char notation_row_count = 0;
static char notation_moves[NOTATION_ROWS][5];
static unsigned char notation_players[NOTATION_ROWS];

/*
 * Veranda port: the panel shows more than the saved log, so it has a log
 * of its own (whole game, or from the saved eleven after a load).
 * log_base is the move number of its first row, 0 when not known.
 */
static unsigned char log_count = 0;
static unsigned char log_base = 1;
static char log_moves[LOG_MAX][5];
static unsigned char log_players[LOG_MAX];

/* Veranda port: the colour map on (options/colour board) */
static unsigned char color_on = 1;

static enum GameStates gameState = STOPPED;
static void_func old_otherPressVec = 0;

/* Application entry points and menu callbacks. */
void NewGameMenuHandler(void);
void AboutMenuHandler(void);
void LoadGameMenuHandler(void);
void SaveGameMenuHandler(void);
void Switch4080MenuHandler(void);
void ColorMenuHandler(void);
void MenuKey(void);
void QuitGame(void);
void MouseClickHandler(void);

void InitScreen(void);
void InitBoard(unsigned char initialPosition);
void InitMovePanel(void);
void NewGame(void);

unsigned char LoadFont(void);

void DrawRect(unsigned char pattern, struct window *square);
void DrawStdRect(unsigned char pattern, struct window *square);

unsigned char GetPieceChar(unsigned char row, unsigned char col);

void RefreshBoardDisplay(void);

void UpdateNotation(
    unsigned char player,
    unsigned char src_row,
    unsigned char src_col,
    unsigned char dest_row,
    unsigned char dest_col
);

void UpdateStatus(const char *message);

void hook_into_system(void);
void remove_hook(void);

/*
 * Veranda port: the menus, in Veranda Look 2's menu bar (the kernel's:
 * theme_menus, Veranda DECISIONS V74, V79): geoChess (About geoChess, the
 * desk accessories of /VERANDA/APPS, Quit), File (New game, Open game...,
 * Save game...), Options (Colour board). Veranda reads GEOS's tables
 * (top, bottom, left, right, flags, then text, type, routine for each
 * item); the bar places its lists itself, only a table's width counts
 * (InitMenus measures them in the UI font).
 *
 * Look 3 "Sunroom": an item whose text starts with $1F starts a group
 * (a dotted line above it; the byte is not drawn), and the text after a
 * TAB is the item's key, drawn at the list's right (the classic look
 * ignores $1F and shows the TAB as a space). The keys are the program's
 * (MenuKey: Ctrl N, O, S, Q).
 */
#define GC_DA_MAX          6

struct gc_item { const char *text; unsigned char type; void *p; };
struct gc_menu1 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct gc_item i[1]; };
struct gc_menu3 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct gc_item i[3]; };
struct gc_menu8 { unsigned char top, bottom; unsigned int left, right; unsigned char flags; struct gc_item i[2 + GC_DA_MAX]; };

/* the accessories' names as the menu shows them: [0] $1F (the first
   starts a group) or a space, then the name (FindFTypes: 17 bytes each) */
#define DA_TEXT            18
static char da_names[GC_DA_MAX * DA_TEXT];
static unsigned char n_da;

static struct gc_menu8 gcMenuV = { 16, 16 + 2 * 16, 0, 0, 2 | VERTICAL, {
    { "About geoChess", MENU_ACTION, AboutMenuHandler },
    { "\x1fQuit\tCtrl Q", MENU_ACTION, QuitGame } } };

static struct gc_menu3 fileMenuV = { 16, 16 + 3 * 16, 0, 0, 3 | VERTICAL, {
    { "New game\tCtrl N", MENU_ACTION, NewGameMenuHandler },
    { "\x1fOpen game...\tCtrl O", MENU_ACTION, LoadGameMenuHandler },
    { "Save game...\tCtrl S", MENU_ACTION, SaveGameMenuHandler } } };

static const char color_on_text[] = "* Colour board";
static const char color_off_text[] = "  Colour board";

static struct gc_menu1 optionsMenuV = { 16, 16 + 16, 0, 0, 1 | VERTICAL, {
    { color_on_text, MENU_ACTION, ColorMenuHandler } } };

static const char *const bar_names[3] = { "geoChess", "File", "Options" };

static struct gc_menu3 mainMenuV = { 0, 16, 0, 0, 3 | HORIZONTAL | UN_CONSTRAINED, {
    { "geoChess", SUB_MENU, &gcMenuV },
    { "File", SUB_MENU, &fileMenuV },
    { "Options", SUB_MENU, &optionsMenuV } } };

#endif