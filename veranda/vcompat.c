/*
 * geoChess on Veranda: cc65's GEOS dialog helpers over DoDlgBox, and a
 * saved game read from its CVT file (see vcompat.h).
 *
 * Not geoChess's code; part of the port (see NOTICE).
 */
#include "vcompat.h"

/* two lines of text and OK: r5, r6 */
struct okdlg {
    unsigned char pos;
    unsigned char t1, x1, y1, reg1;
    unsigned char t2, x2, y2, reg2;
    unsigned char ok, okx, oky;
    unsigned char end;
};
static const struct okdlg okbox = {
    DEF_DB_POS | 1,
    DBVARSTR, TXT_LN_X, TXT_LN_2_Y, 0x0C,
    DBVARSTR, TXT_LN_X, TXT_LN_3_Y, 0x0E,
    OK, DBI_X_0, DBI_Y_2,
    0
};

unsigned char DlgBoxOk(const char *line1, const char *line2)
{
    r5 = (unsigned int)line1;
    r6 = (unsigned int)line2;
    return DoDlgBox((void *)&okbox);
}

/*
 * "Name.cvt" (docs/APPFORMAT.md): the directory entry's GEOS type at 22,
 * the info block from 254 (its class at 329), a sequential file's data
 * from 508
 */
unsigned char vc_read_data(const char *name, const char *cls, void *buf, unsigned int size)
{
    static char path[VRF_NAME + 6];
    static unsigned char head[24];
    static char cl[21];
    unsigned char h;
    unsigned int n;

    if (strlen(name) > VRF_NAME - 4)
        return 5;
    strcpy(path, name);
    strcat(path, ".cvt");
    h = FOpen(path, 'r');
    if (h < 2)
        return 5;                       /* FILE_NOT_FOUND */
    n = FRead(h, head, sizeof head);
    cl[20] = 0;
    if (n != sizeof head || head[22] != APPL_DATA || FSeek(h, 329, 0) != 0
        || FRead(h, cl, 20) != 20 || strncmp(cl, cls, strlen(cls)) != 0) {
        FClose(h);
        return VC_NOT_GAME;
    }
    if (FSeek(h, 508, 0) != 0) {
        FClose(h);
        return 5;
    }
    n = FRead(h, buf, size);
    FClose(h);
    return n == size ? 0 : 7;
}
