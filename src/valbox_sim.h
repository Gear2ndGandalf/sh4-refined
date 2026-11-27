// v3.58 MN358: the Options value box, the pure part of src\valbox.inc (test\valbox_test.c, test\build_valbox_test.cmd).
//
// USER_FEEDBACK_v357: "Option parameter white arrows will jump out right-wards when the parameter name is too long. This is
// jarring when using a mouse to click on the arrow. Make it so all parameter arrows stay aligned throughout all the names."
// Before (v3.33 .. v3.57): the value was left-aligned at x 364 and the right arrow sat 6 px after the ROW's widest value
// (arrowfit_sim.h AfRowShift, at most +36), a long value moved the column (LongValW), Resolution kept GOG's +20 for its
// wide labels: so the right arrow stood at a different x on almost every row (the v3.57 log: page 3 row 0 +24, page 4 row
// 4 +19, row 6 +16 ...) and Resolution's moved with its value.
// Now: every value row of a PAGE shares one box. The box is wide enough for the widest value of the page (every value of
// every row, measured with the game's font metrics, LabelW: the four HD fonts keep the stock metrics, so one measure holds
// for all five fonts), at least the stock box (left arrow 344, right arrow 464); it grows to the right first (the right
// arrow at most at x 500: it ends by 508 of the 512 canvas), then to the left (never into the page's widest label). Each
// value is drawn centred in it; both arrows sit at its edges for every value of every row; the mouse rects follow.
// The canvas is 512 x 448 at every Aspect Ratio (Menus43 squeezes it into the 4:3 middle; the arrows and their rects go
// through the same mapping), so the box is the same at 4:3, 16:9, 21:9 and 32:9.
//
// Geometry, relative to X0 (the stock value x: 512 - 40 - 100 - 8 = 364, + the page's slide offset):
//   the left arrow (8 x 24) at X0 + l - 8 .. X0 + l, the right arrow at X0 + r .. X0 + r + 8, the value centred in l .. r;
//   the stock: l = -12, r = 100 (span 112). The stock draw puts the left arrow at col - 20 and the right one at col + 100
//   (+ the shift Refined adds at 0x52587C), col = the value x: so col = X0 - move, move = -12 - l, shift = (r - l) - 112.
//   Mouse rects (0x525AA0): left {X0 + l - 8, 20}, value {X0 + l + 12, r - l - 12}, right {X0 + r, 20}: the stock ones moved.
#ifndef VALBOX_SIM_H
#define VALBOX_SIM_H

#define VBX_X0 364         // the stock value x at rest (0x525644: canvas 512 - 40 - 100 - 8)
#define VBX_ARROW_W 8      // the value arrows are 8 x 24 (0x523620, size 1.0)
#define VBX_L0 (-12)       // the stock left arrow's right end, from X0 (the arrow at X0 - 20)
#define VBX_R0 100         // the stock right arrow's x, from X0
#define VBX_SPAN0 (VBX_R0 - VBX_L0)  // 112: the stock room between the arrows
#define VBX_PAD 6          // at least this much air between a value and each arrow (the v3.33 AF_GAP)
#define VBX_R_MAX 136      // the right arrow at most at X0 + 136 = 500: it ends by canvas x 508 (arrowfit_sim.h's 464 + 36)
#define VBX_LABEL_X 40     // the labels' x (the row rects, 0x5254F0)
#define VBX_LABEL_GAP 10   // a label's end .. the left arrow, at least
#define VBX_HIT_W 20       // the stock arrow rects are 20 wide (the arrow + 12 of air towards the value)

typedef struct {
    int l, r;       // the left arrow's right end / the right arrow's x, from X0
    int move;       // the value column's move to the left (the stock draw's col = X0 - move)
    int shift;      // the right arrow's shift after the move (0x52587C: col + 100 + shift)
    int need;       // the room the widest value needs (its width + 2 x VBX_PAD)
    int fits;       // 1 = every value fits between the arrows with VBX_PAD of air
} VbxBox;

static int VbxCeilI(float w)  // ceil for any sign (no CRT)
{
    int i = (int)w;
    if ((float)i < w) ++i;
    return i;
}
static int VbxFloorI(float w)
{
    int i = (int)w;
    if ((float)i > w) --i;
    return i;
}

// The page's box from its widest value and its widest label (canvas px; 0 = none)
static void VbxMake(VbxBox *b, float widestVal, float widestLabel)
{
    const int need = (widestVal > 0.0f ? VbxCeilI(widestVal) : 0) + 2 * VBX_PAD;
    const int span = need > VBX_SPAN0 ? need : VBX_SPAN0;
    int l = VBX_L0, r = VBX_L0 + span;
    const int lmin = VbxCeilI((float)VBX_LABEL_X + (widestLabel > 0.0f ? widestLabel : 0.0f)) - VBX_X0 + VBX_LABEL_GAP + VBX_ARROW_W;
    if (r > VBX_R_MAX) {  // grow to the right first, then to the left
        r = VBX_R_MAX;
        l = r - span;
    }
    if (l < lmin) l = lmin < VBX_L0 ? lmin : VBX_L0;  // never into the labels (never narrower than the stock box)
    b->l = l;
    b->r = r;
    b->move = VBX_L0 - l;
    b->shift = (r - l) - VBX_SPAN0;
    b->need = need;
    b->fits = r - l >= need;
}
// the stock box (the switch off, or nothing measured)
static void VbxStock(VbxBox *b) { VbxMake(b, 0.0f, 0.0f); }

// The x a value w px wide is drawn at (centred between the arrows); col = the column x the draw passes (X0 - move,
// with the slide offset): the left arrow ends at col - 12 (= X0 + l)
static int VbxTextX(const VbxBox *b, int col, float w)
{
    return col - 12 + VbxFloorI(((float)(b->r - b->l) - w) * 0.5f + 0.5f);
}
// the arrows where the stock draw puts them after Refined's move / shift (col = X0 - move)
static int VbxLeftArrowX(const VbxBox *b, int x0) { return x0 - b->move - 20; }
static int VbxRightArrowX(const VbxBox *b, int x0) { return x0 - b->move + VBX_R0 + b->shift; }

// The mouse rects of the cursor row (x, w; the stock ones 344 / 20, 364 / 100, 464 / 20 at x0 = 364)
static void VbxRects(const VbxBox *b, int x0, int left[2], int val[2], int right[2])
{
    left[0] = x0 + b->l - VBX_ARROW_W, left[1] = VBX_HIT_W;
    val[0] = x0 + b->l + 12, val[1] = b->r - b->l - 12;
    right[0] = x0 + b->r, right[1] = VBX_HIT_W;
}
// ... as the deltas the 0x525CE3 stub adds to the stock rects: left x, value x, value w, right x
static void VbxRectDeltas(const VbxBox *b, int *dLeft, int *dVal, int *dValW, int *dRight)
{
    *dLeft = -b->move;
    *dVal = -b->move;
    *dValW = b->shift;
    *dRight = b->shift - b->move;
}

// Two boxes alike (the log line is written again only when a page's box changes)
static int VbxSame(const VbxBox *a, const VbxBox *b) { return a->l == b->l && a->r == b->r; }

// The pause Extra Menu > Camera page (extramenu.inc pm_cam_draw, Refined's own pause draw; canvas units): the value column
// starts at vx (pm_eo_x + 0x12), "<" stays where it was (pm_eo_x - 4). Before, ">" stood 6 px after the CURRENT value
// (it moved with every step); now the page has one room, its widest value, every value is centred in vx .. vx + room and
// ">" stands 6 px after the room for every value of every row. The room stops where ">" would pass x 500.
#define VBX_PM_GAP 6
#define VBX_PM_RIGHT 500
static int VbxPmRoom(int vx, float widest)
{
    int w = widest > 0.0f ? VbxCeilI(widest) : 0;
    if (vx + w + VBX_PM_GAP > VBX_PM_RIGHT) w = VBX_PM_RIGHT - VBX_PM_GAP - vx;
    return w < 0 ? 0 : w;
}
static int VbxPmGtX(int vx, int room) { return vx + room + VBX_PM_GAP; }
static int VbxPmTextX(int vx, int room, float w) { return vx + VbxFloorI(((float)room - w) * 0.5f + 0.5f); }

#endif
