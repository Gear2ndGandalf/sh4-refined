// ================================================================ v3.58 MN358: the Options value box (valbox_sim.h)
// USER_FEEDBACK_v357 issue: "Option parameter white arrows will jump out right-wards when the parameter name is too long.
// This is jarring when using a mouse to click on the arrow. Make it so all parameter arrows stay aligned throughout all the
// names." Every value row of a page shares one box (valbox_sim.h VbxMake: the page's widest value, measured with LabelW);
// each value is drawn centred in it and both arrows sit at its edges, on every row of the page and for every value.
// Included from sh4windowmod.c before the 0x52570E / 0x525CE3 stubs. The sites it rides on are Refined's since v3.19 - v3.33:
//   0x52570E  the value column (hkValueCol): the column moves VbxBox.move to the left (the left arrow at col - 20)
//   0x525802  the value text (hkRowValue): the value's x = centred in the box (VbxValueX)
//   0x52587C  the right arrow (hkArrowDraw): + VbxBox.shift (ArrowShift)
//   0x525CE3  the cursor row's mouse rects (hkMouseCol): the left / value / right rects where the arrows are drawn
// and two new 5-byte sites for the volume bars' arrows (Music / Effects Volume keep their bars; their arrows line up with
// the page's box): 0x5256B6 and 0x5256E2, each `68 80 80 80 80` (push 80808080h, the arrows' colour) -> `E8 rel32` to a
// stub that moves ebp (the bar's x, from which 0x5256C0 / 0x5256EC place the arrows) and pushes the colour itself. Both or
// neither (byte-checked, rolled back, listed in SH4WM_Patches through Poke; clear of the cheat table and Playable Eileen).
// Hidden [Options] ValueBox=0: v3.57's arrows (the row's widest value, the long-value column move, Resolution's +20).
// Needs the themed layout (g_olOn) and the four hooks above; else the old rules, said once.
#include "valbox_sim.h"

static int g_vbxOn;                  // 1 = the box rules (VbxInstall)
static int g_vbxBarOn;               // 1 = the bar arrows' two sites are ours
static int g_vbxHooks;               // the hooks it rides on: 1 = 0x52570E + 0x525CE3, 2 = 0x525802
static VbxBox g_vbxBox[LP_MAX];      // each live page's box (measured at its first draw after LoadSlot)
static float g_vbxWidest[LP_MAX];    // its widest value (it only grows until the page is loaded again)
static float g_vbxLabel[LP_MAX];     // its widest label
static signed char g_vbxHave[LP_MAX];
static VbxBox g_vbxSaid[LP_MAX];     // the box last logged per page (a line only when it changes)
static signed char g_vbxSaidHave[LP_MAX];
static volatile LONG g_vbxQ, g_vbxQFault, g_vbxQGrow;
static DWORD g_vbxBarMove, g_vbxBarShift;  // the bar stubs' operands for the row being drawn (VbxRowNote; 0 = stock)
static int g_mouseValDx, g_mouseValW;  // the 0x525CE3 stub's value-rect deltas (the long-value path: 0)

static int VbxActive(void) { return g_vbxOn && g_olOn; }
// extramenu.inc pm_cam_draw: the pause Camera page's fixed room ([Options] ValueBox alone: it needs no Options hook)
static int VbxPmOn(void)
{
    static int s_on = -1;
    if (s_on < 0) s_on = GetPrivateProfileIntA("Options", "ValueBox", 1, g_iniPath) != 0;
    return s_on;
}

static const BYTE *VbxText(DWORD id) { return ((const BYTE *(__cdecl *)(int, int))0x50C440)(0, (int)id); }

// a row's widest value (0 = no value to measure: a bar, a folder, an empty table); *nv = its values
static float VbxRowWidest(int lp, int row, int *nv)
{
    const DWORD *r = g_lpTable[lp] + 7 * row;
    const DWORD *ids = (const DWORD *)(ULONG_PTR)r[3];
    int n = (int)r[4], i, v;
    float m = 0.0f;
    *nv = 0;
    if (!ids || n <= 0 || !r[1] || VbBarRule(r[0], r[5])) return 0.0f;  // (the bars keep their bars: no text to measure)
    v = ((int(__cdecl *)(void))(ULONG_PTR)r[1])();
    if (v < 0) return 0.0f;  // a value-less row (a folder's {0}, the stock "Press")
    if (n > 256) n = 256;    // (no row has so many: the number rows are one text each)
    for (i = 0; i < n; ++i) {
        const float w = LabelW(VbxText(ids[i]));
        if (w > m) m = w;
    }
    *nv = n;
    return m;
}

static void VbxSay(int lp, int rows, int vals)
{
    const VbxBox *b = &g_vbxBox[lp];
    if (g_vbxSaidHave[lp] && VbxSame(&g_vbxSaid[lp], b)) return;
    g_vbxSaid[lp] = *b;
    g_vbxSaidHave[lp] = 1;
    if (!LogQuota(&g_vbxQ, 96)) return;
    LogV("options: value box: page %d (%s): %d value row(s), %d value(s), the widest %.0f px, the widest label %.0f px: the "
         "arrows at x %d and %d on every row (%s)%s", lp, kOlPage[lp].title, rows, vals, g_vbxWidest[lp], g_vbxLabel[lp],
         VbxLeftArrowX(b, VBX_X0), VbxRightArrowX(b, VBX_X0),
         !b->move && !b->shift ? "the stock places" : "the box grown for the widest value",
         b->fits ? "" : "; a value is wider than the room left by the labels: drawn centred, under the arrows");
}

static void VbxMeasure(int lp)
{
    const DWORD *t = g_lpTable[lp];
    const int n = g_lpCount[lp];
    float wv = 0.0f, wl = 0.0f;
    int i, rows = 0, vals = 0;
    for (i = 0; i < n && i < 64; ++i) {
        int nv;
        float w;
        if (t[7 * i + 5]) {
            w = LabelW(VbxText(t[7 * i + 5]));
            if (w > wl) wl = w;
        }
        w = VbxRowWidest(lp, i, &nv);
        if (nv) ++rows, vals += nv;
        if (w > wv) wv = w;
    }
    g_vbxWidest[lp] = wv;
    g_vbxLabel[lp] = wl;
    VbxMake(&g_vbxBox[lp], wv, wl);
    g_vbxHave[lp] = 1;
    VbxSay(lp, rows, vals);
}

// the box of live page lp (measured now if it is not yet); NULL = the old rules
static const VbxBox *VbxOf(int lp)
{
    if (!VbxActive() || (unsigned)lp >= LP_MAX || !g_lpTable[lp] || g_lpCount[lp] <= 0) return NULL;
    if (!g_vbxHave[lp]) {
        __try {
            VbxMeasure(lp);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            VbxStock(&g_vbxBox[lp]);
            g_vbxHave[lp] = 1;
            if (LogQuota(&g_vbxQFault, 3)) LogAnomaly("options", "value box: measuring page %d faulted: the stock box there", lp);
        }
    }
    return &g_vbxBox[lp];
}
// LoadSlot: the page is shown again (its values are measured again at its first draw)
static void VbxForget(int lp)
{
    if ((unsigned)lp < LP_MAX) g_vbxHave[lp] = 0;
}
// look.inc: a folder row's triangle stands where the page's values start (the left arrow's place + 20)
static int VbxColMove(int lp)
{
    return VbxActive() && (unsigned)lp < LP_MAX && g_vbxHave[lp] ? g_vbxBox[lp].move : 0;
}
// screenfx.inc: the value panel of a preview page reaches round the page's arrows (x0 / x1: the stock 330 / 478)
static void VbxPanelX(int lp, float *x0, float *x1)
{
    if (!VbxActive() || (unsigned)lp >= LP_MAX || !g_vbxHave[lp]) return;
    if ((float)(VBX_X0 + g_vbxBox[lp].l - VBX_ARROW_W - 14) < *x0) *x0 = (float)(VBX_X0 + g_vbxBox[lp].l - VBX_ARROW_W - 14);
    if ((float)(VBX_X0 + g_vbxBox[lp].r + VBX_ARROW_W + 6) > *x1) *x1 = (float)(VBX_X0 + g_vbxBox[lp].r + VBX_ARROW_W + 6);
}

// MouseColC: the box the cursor row's mouse rects follow; a volume bar's arrows stay where the game puts them when the two
// bar sites are not ours (the stock box: the stock rects)
static const VbxBox *VbxMouseBox(const VbxBox *b, int row)
{
    static VbxBox s_stock;
    const DWORD *r;
    if (g_vbxBarOn || (unsigned)g_lpage >= LP_MAX || !g_lpTable[g_lpage] || (unsigned)row >= (unsigned)g_lpCount[g_lpage]) return b;
    r = g_lpTable[g_lpage] + 7 * row;
    if (!VbBarRule(r[0], r[5])) return b;
    VbxStock(&s_stock);
    return &s_stock;
}
// RowChangedDraw (each row, before 0x5254F0 draws it): the box of the row's page, for the hooks of this row's draw
static void VbxRowNote(int slot)
{
    const VbxBox *b = (unsigned)slot < 3 ? VbxOf(g_slotPage[slot]) : NULL;
    g_vbxRow = b;
    g_vbxBarMove = b && g_vbxBarOn ? (DWORD)b->move : 0;
    g_vbxBarShift = b && g_vbxBarOn ? (DWORD)b->shift : 0;
}

// 0x525802 (hkRowValue): the x the value text is drawn at; x = the column (X0 - move, with the slide offset)
static int __cdecl VbxValueX(int id, int x)
{
    const VbxBox *b = g_vbxRow;
    if (!b || !id) return x;
    __try {
        const float w = LabelW(VbxText((DWORD)id));
        const int lp = (unsigned)g_drawSlot < 3 ? g_slotPage[g_drawSlot] : -1;
        if ((unsigned)lp < LP_MAX && b == &g_vbxBox[lp] && w > g_vbxWidest[lp] + 0.5f) {
            // a value wider than the page's measure (a text that changes with its value, the ini's own font name): the
            // box grows for it (once: the page keeps the widest until it is loaded again)
            const VbxBox old = *b;
            g_vbxWidest[lp] = w;
            VbxMake(&g_vbxBox[lp], w, g_vbxLabel[lp]);
            if (!VbxSame(&old, b) && LogQuota(&g_vbxQGrow, 20))
                LogV("options: value box: page %d row %d: a %.0f px value is wider than the page's measure: the arrows at x %d "
                     "and %d from now on", lp, g_drawRow, w, VbxLeftArrowX(b, VBX_X0), VbxRightArrowX(b, VBX_X0));
            g_vbxSaid[lp] = *b;
            g_vbxSaidHave[lp] = 1;
        }
        return VbxTextX(b, x, w);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (LogQuota(&g_vbxQFault, 3)) LogAnomaly("options", "value box: measuring a value faulted: drawn where the column is");
    }
    return x;
}

