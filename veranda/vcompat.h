/*
 * geoChess on Veranda: what cc65's GEOS target gave geochess.c that
 * Veranda's C SDK spells differently.
 *
 * Veranda's calls are GEOS's with the registers as parameters, so most
 * of cc65's GEOS calls map one to one. The differences:
 *
 * - argument order: cc65 passes y before x (PutString(s, y, x),
 *   HorizontalLine(pattern, y, x1, x2)); Veranda's wide calls take x
 *   first and a word for y;
 * - a struct window is drawn through InitDrawWindow + Rectangle in cc65;
 *   here it is passed to the wide call directly (vc_rect, vc_invert);
 * - cc65's dialog helper DlgBoxOk is library code over DoDlgBox; it is
 *   written here the same way (the saved games' names are asked and
 *   chosen in Veranda's Open and Save dialog: vrchoose.h, long names);
 * - ReadFile follows a block chain, which Veranda does not have: a saved
 *   game is read from its CVT file as a stream (vc_read_data), its GEOS
 *   type and class checked there;
 * - SaveFile's info block is Veranda's (docs/APPFORMAT.md), so the save
 *   handler fills it with vc_fileheader.
 *
 * Not geoChess's code; part of the port (see NOTICE).
 */
#ifndef VCOMPAT_H
#define VCOMPAT_H

#include <veranda.h>
#include <vr.h>
#include <string.h>
#include <theme.h>      /* Veranda Look 2: the palette, the menu bar */
#include <vrwin.h>      /* the SDK's window chrome (the move log) */
#include <vrchoose.h>   /* the Open and Save dialog */
#include <vrpath.h>     /* the card's folders (the desk accessories) */

/* geoChess's own LoadFont (its piece font) is not Veranda's LoadFont call
   (API level 3, the font service): renamed for the game */
#define LoadFont gc_LoadFont

typedef void (*void_func)(void);

/* cc65's struct window, with word rows (the 640x480 screen) */
struct window {
    unsigned int top, bot;
    unsigned int left, right;
};

#define vc_rect(w)    WRectangle((w)->left, (w)->top, (w)->right, (w)->bot)
#define vc_invert(w)  WInvertRectangle((w)->left, (w)->top, (w)->right, (w)->bot)
#define vc_inside(w)  WIsMseInRegion((w)->left, (w)->top, (w)->right, (w)->bot)

/* cc65's argument order (y first) onto the wide calls */
#define PutString(s, y, x)            WPutString((void *)(s), (x), (y))
#define PutChar(c, y, x)              WPutChar((c), (x), (y))
#define HorizontalLine(p, y, x1, x2)  WHorizontalLine((p), (x1), (x2), (y))
#define VerticalLine(p, y1, y2, x)    WVerticalLine((p), (x), (y1), (y2))
#define LoadCharSet(f)                LoadCharSet((void *)(f))
#define DoMenu(m)                     DoMenu((void *)(m), 0)
#define SetNewMode()                  /* GEOS 128's 40/80 switch: never called */

/* GEOS file types and the CBM type of a document */
#define USR             3
#define SEQUENTIAL      0

/* cc65's dialog helper */
unsigned char DlgBoxOk(const char *line1, const char *line2);

/* a sequential APPL_DATA file's data (from its CVT file "name.cvt", a long
   name): 0, 5 (FILE_NOT_FOUND), 7 (short), VC_NOT_GAME (another GEOS type,
   or a class not starting with cls) */
#define VC_NOT_GAME     10
unsigned char vc_read_data(const char *name, const char *cls, void *buf, unsigned int size);

/*
 * Veranda's info block (docs/APPFORMAT.md), the 256 bytes SaveFile is
 * given: bytes 0-1 the file's name.
 */
struct vc_fileheader {
    const char *name;
    unsigned char icon_w, icon_h;
    unsigned char icon_pic[64];        /* $BF, then 63 bytes */
    unsigned char dostype;
    unsigned char type;
    unsigned char structure;
    unsigned int load_address;
    unsigned int end_address;
    unsigned int exec_address;
    char class_name[20];
    char author[20];
    char parent[20];
    unsigned char appdata[23];
    char note[96];
};

#endif
