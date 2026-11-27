// v3.56 MN356 (USER_FEEDBACK_v355 issue 1: "Sometimes, submenus cannot be accessed via mouse click on the blue right
// arrow. Only the Enter key works all the time."): the pure part of src\menuclick.inc, tested by test\folderclick_test.c.
//
// Why clicks were dropped (the stock page input 0x525AA0, disassembled from the stock exe; BUILD_NOTES):
//   1. The page input calls the cursor row's input only when the mouse is on THAT row's value column (x 364 .. 464),
//      its left arrow (344 .. 364) or right arrow (464 .. 484) and the left button went down this frame (0x405490), or on
//      a key. A click on a row's label (x 40 .. 344) is never a click; a click on another row than the cursor's is never
//      a click either (the cursor follows the mouse only on frames the mouse moves and nothing is pressed: 0x405850).
//   2. Refined's folder inputs (layout.inc's folders -> gameplay.inc GpOpen, Back -> GpBackIn, the StockMoves=0 combo
//      Subtitles -> SubRowInput) opened only on the confirm action (0x4F84A0(0, 0x11)), which no mouse button feeds.
//      The stock screen rows (Brightness, Gamma, Buttons: 0x524900 / 0x525100 / 0x524940) test 0x405490 too: they opened
//      on a click on their value column, the folders never did ("sometimes").
//   3. A click during a page slide or the fade never reaches the page input (0x526490 runs it only at rest).
// The fix: a click on a folder-kind row opens it wherever on the row it lands (its label, the triangle, the value column,
// the arrows' places), whatever row the cursor is on; a combo row (a value AND a page) opens on its label only (its value
// and arrows keep changing the value); a click mid-slide / mid-fade is logged once and the next one works.
#ifndef FOLDERCLICK_SIM_H
#define FOLDERCLICK_SIM_H

#define FC_COL_X 344    // the left arrow's x: the value column's area starts here (0x525C1B: 512 - 40 - 100 - 8 - 20)
#define FC_RIGHT 484    // the right arrow's end (464 + 20)
#define FC_ROW_X 40     // the rows' x (the stock row rects)
#define FC_PAD_L 8      // the open box starts this much left of the row (a click on the label's first letter edge, after the
                        // window's truncating scale, stays inside; nothing is drawn there)

enum { FC_C_NONE, FC_C_ROW, FC_C_LABEL };  // a row's click class: none (stock), the whole row, its label only
enum { FC_STOCK, FC_OPEN, FC_REFUSE };

// the box a click opens the row in (canvas units, 512 x 448); 0 = none. rx = the row rect's x (40): the box runs from
// rx - FC_PAD_L to the right arrow's end (a combo's: to the value column)
static int FcBox(int cls, int rx, int ry, int rh, int box[4])
{
    if (cls == FC_C_NONE || rh <= 0) return 0;
    box[0] = rx - FC_PAD_L;
    box[1] = ry;
    box[2] = (cls == FC_C_LABEL ? FC_COL_X : FC_RIGHT) - box[0];
    box[3] = rh;
    return box[2] > 0;
}

// the row the mouse is on, as the stock hover finds it (0x525EAF .. 0x525FA5: each row's rect from x 40, 424 wide, the
// row's own y and height): the same box for every row (a folder's reaches the right arrow's end, so its own box is used)
#define FC_HOVER_RIGHT 464
static int FcHoverBox(int rx, int ry, int rh, int box[4])
{
    if (rh <= 0) return 0;
    box[0] = rx, box[1] = ry, box[2] = FC_HOVER_RIGHT - rx, box[3] = rh;
    return box[2] > 0;
}
// the stock hover's rule for overlapping rects: the last row that holds the point wins (0x525F4F)
static int FcPickByHit(const int *inside, int n)
{
    int i, hit = -1;
    for (i = 0; i < n; ++i)
        if (inside[i]) hit = i;
    return hit;
}

// click: the left button went down this frame; hit: the row under it (FcPickByHit over every row's hover box or open
// box, -1 none); open: the point is in that row's open box (FcBox: a folder-kind row; a combo's label); busy: a slide, a
// fade or a page request is pending
static int FcDecide(int click, int hit, int open, int busy)
{
    if (!click || hit < 0 || !open) return FC_STOCK;
    return busy ? FC_REFUSE : FC_OPEN;
}

// The canvas <-> window mapping of Refined's menu mouse (sh4windowmod.c ToIface / FromIface / HitRectI, Menus43): the
// menus are squeezed into the 4:3 middle, s = (width / height) / (4 / 3) (1 at 4:3). HitRectI hands the game's int test
// 0x405690 the rect in the window's 512-wide space; 0x405690 scales it to the client (truncating) and tests strictly
// inside (0x405690: x > left && x < right && y > top && y < bottom).
static float FcFromIface(float x, float s) { return 256.0f + (x - 256.0f) / s; }
static float FcToIface(float x, float s) { return 256.0f + (x - 256.0f) * s; }
static int FcHitClient(const int rc[4], float s, int cw, int ch, int mx, int my)
{
    int r0, r2;
    long left, top, right, bottom;
    r0 = (int)(FcFromIface((float)rc[0], s) + 0.5f);  // (floorf(x + 0.5) for x >= 0: the menus' rects are on the canvas)
    r2 = (int)((float)rc[2] / s + 0.5f);
    left = (long)((float)(r0 * cw) * (1.0f / 512.0f));
    top = (long)((float)(rc[1] * ch) * (1.0f / 448.0f));
    right = (long)((float)((r0 + r2) * cw) * (1.0f / 512.0f));
    bottom = (long)((float)((rc[1] + rc[3]) * ch) * (1.0f / 448.0f));
    return mx > left && mx < right && my > top && my < bottom;
}
// the client pixel a canvas point is drawn at (the 2D squeeze of Menus43)
static void FcDrawnAt(float cx, float cy, float s, int cw, int ch, int *mx, int *my)
{
    *mx = (int)(FcFromIface(cx, s) * (float)cw / 512.0f);
    *my = (int)(cy * (float)ch / 448.0f);
}

#endif
