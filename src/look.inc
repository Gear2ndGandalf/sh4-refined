// ================================================================ v3.53 OL2: the Options look (round R2 of the Options layout)
// Spec: staging/options_layout/PLAN.md sections 3 (visual language) and 9 (R2), its data tools/layout_data.py (DEPS), the
// brief staging/v353_layout/w3_look/BRIEF.md. Pure code: look_sim.h (test\look_sim_test.c). No exe site of its own: it draws
// from the per-row hook RowChangedDraw (0x523AA0's row-draw caller), recolours through RowLabelC / RowValueC (0x525553 /
// 0x525802), names pages through LoadSlot, and swaps fields of Refined's own row records (never the exe's stock tables).
//   Folders  a value-less row that opens a page (getter 0x51EB10 and SubmenuRowPage: Brightness, Gamma, Button Config,
//            Option 4's Room 302, Option 5's folders) gets a blue right triangle in the value column, 0x523620(364 + the
//            slide offset, y + 4, dir 3, size 1.0, blue; deep blue under the cursor). Its value table becomes {0}: the stock
//            "Press" of a getter -1 row (0x5255EC, x 364 on the cursor row) would draw under the triangle, and id 0 draws
//            nothing (0x5239A0 `test esi, esi`). Combo rows (Subtitles, Film Grain, Shadows, Sound Mode: a value and Enter)
//            keep their value and arrows: the blue label only (R5 makes them folders).
//   Greyed   the rows of layout_data.py's DEPS that exist today, while their switch is off: grey label and value (0x484848,
//            0x303060 under the cursor), inert (the row's input through a thunk that does nothing) and a description that
//            starts with the reason ("Needs Film Grain on.", then the row's own line: one line of at most 52 characters,
//            v3.53 MERGE). The camera's DEPS rows live in the pause Extra Menu > Camera
//            page today (extramenu.inc pm_cam_*: LkCamRowGrey). Playable Eileen's owner greys stay as they are.
//   Strip    under the title (y 68, font 1) the names of the pages in the cycle (0 .. LP_N - 1, today's six with their R1
//            titles, read whole: v3.53 MERGE): the page shown white and underlined in the cursor red (the rect queue
//            0x557300), on a sub page its top page mid grey, the rest dark grey. Drawn with each slot's page and slide offset.
//   Titles   a hidden page shows "Parent > Page" (the last two levels; at most 28 characters and room before the top right
//            arrow at x 464, else its own title).
//   Glyphs   the text queue holds 800 glyphs a frame (5570B0). One verbose line per page (the first time it settles in a
//            session, again only when it grows): the estimate of title + strip + rows + description, twice for the shadow
//            strings, + the cursor's flash, and the queue measured right after the description; ANOMALY over 800.
// Hidden off switch: [Options] Look=0 (v3.52's look: no strip, triangle, crumb or new greys).
#include "look_sim.h"

enum { LK_T0 = 512, LK_R0 = LK_T0 + 24, LK_D0 = LK_R0 + 8, LK_END = LK_D0 + 8 };  // MSG_V53 + 512 .. 575 (BRIEF_COMMON: w3's)
typedef char LkIdsFit[LK_END <= 576 && LP_MAX <= 24 && LKR_N <= 8 ? 1 : -1];
typedef char LkBlockFit[MSG_V53 + 576 == MSG_V54 ? 1 : -1];  // the block before v3.54's  // v3.54 OL34

#define LK_STRIP_Y 68         // the strip's top: the title is font 0 at y 36 (0x5259C3), row 0 at y 96
#define LK_STRIP_ROOM 496.0f  // x 8 .. 504 of the 512 canvas
#define LK_FOLDER_X 364.0f    // the value column (0x525644: 512 - 40 - 100 - 8)
#define LK_TITLE_X 140        // the title's x (0x5259A5: arg + 8Ch)
#define LK_TITLE_RIGHT 456.0f // the top right page arrow starts at x 464 (0x526700: 512 - 40 - 8)
#define LK_COL_ON 0x808080u   // the page shown: white
#define LK_COL_TOP 0x505050u  // a sub page's top page: mid grey
#define LK_COL_OFF 0x2C2C2Cu  // the other pages: dark grey
#define LK_COL_LINE 0x404060u // the underline: the cursor's red (the label colour 0x80404060 at 0x525A2A)

static int g_lkOn;
static BYTE g_lkTitle[24][LK_TXT + 8];   // "Parent > Page" of each hidden page (LK_T0 + lp)
static DWORD g_lkTitleShown[24];         // the title id each page shows (LoadSlot)
static BYTE g_lkReason[LKR_N][LK_TXT];   // LK_R0 + r
static BYTE g_lkDesc[8][768];            // LK_D0 + k: reason + the row's own description
static BYTE g_lkEmpty[4] = { 0x00, 0x80, 0xFF, 0xFF };
static DWORD g_lkNoVal[2] = { 0, 0 };    // a folder row's value table (id 0: nothing drawn)
static BYTE g_lkStrip[LP_N][LK_TXT];     // the strip's names (rebuilt as each Options opening first draws)
static int g_lkStripOk;
static int g_lkLogged[24], g_lkLastLp = -1, g_lkStable;  // the glyph lines
// v3.55 OL56 (R6): the strip's names as last drawn at rest on the page shown (the click's boxes); -1 = none this opening
static float g_lkHitX[LP_N], g_lkHitW[LP_N], g_lkHitY, g_lkHitH;
static int g_lkHitLp = -1;
static volatile LONG g_lkStripGo = -1;  // the top page a click asked for (PageKeyFade takes it)

static const BYTE *LkText(int id)
{
    const int i = id - MSG_V53;
    if (i >= LK_T0 && i < LK_T0 + 24) return g_lkTitle[i - LK_T0][0] == 0x00 && g_lkTitle[i - LK_T0][1] == 0x80 ? g_lkTitle[i - LK_T0] : g_lkEmpty;
    if (i >= LK_R0 && i < LK_R0 + LKR_N) return g_lkReason[i - LK_R0];
    if (i >= LK_D0 && i < LK_D0 + 8) return g_lkDesc[i - LK_D0][0] == 0x00 && g_lkDesc[i - LK_D0][1] == 0x80 ? g_lkDesc[i - LK_D0] : g_lkEmpty;
    return g_lkEmpty;
}

// a string's width in canvas px in font `font` at letter spacing sp (LabelW with the font as a parameter)
static float LkTextW(const BYTE *s, int font, float sp)
{
    const BYTE *f = *(const BYTE *const *)(ULONG_PTR)(0x1261B98 + 4 * (font & 3));
    float u = 0.0f;
    int i, n = 0;
    if (!s) return 0.0f;
    i = s[0] == 0x00 && s[1] == 0x80 ? 2 : 0;
    while (n < 128) {
        const BYTE c = s[i];
        if (c == 0xFF) {
            if (s[i + 1] == 0xFF) break;
            i += 2;
            continue;
        }
        if (c == 0xE0) u += 13.0f + sp, i += 3;
        else if (c == 0xF0) u += 35.0f, i += 2;
        else if (c == 0xFC) u -= s[i + 1], i += 2;
        else if (c == 0xFE) u += s[i + 1], i += 2;
        else {
            if (!f) u += 13.0f + sp;
            else if (c == 0x00) u += f[0x10] + sp;
            else if (c < 0xE0) {
                const int g = 0xE0 - c, k = f[0xF0 + g];
                int lo = k & 0x0F, hi = k >> 4;
                if (lo >= 8) lo -= 16;
                if (hi >= 8) hi -= 16;
                u += (float)(f[0x10 + g] + lo + hi) + sp;
            } else {
                u += 13.0f + sp;
            }
            ++i;
        }
        ++n;
    }
    return u / 1.25f;
}
static float LkSpacing(void)
{
    const float sp = *(volatile float *)0x1261C20;
    return sp > 0.0f && sp < 20.0f ? sp : 2.5f;
}
static const BYTE *LkMsg(DWORD id) { return ((const BYTE *(__cdecl *)(int, int))0x50C440)(0, (int)id); }

// ---------------------------------------------------------------- the page tree (v3.54 OL34: PLAN section 6's ids, layout_sim.h)
static int LkParentOf(int lp)
{
    // v3.54 OL34: every hidden page has its parent in g_lpParent (LP_SUB / LP_FX moved onto REQ_OPEN)
    return lp >= LP_N && lp < LP_MAX ? g_lpParent[lp] : -1;
}
static int LkTopOf(int lp)
{
    int i;
    for (i = 0; i < 4 && lp >= LP_N; ++i) lp = LkParentOf(lp);
    return lp >= 0 && lp < LP_N ? lp : -1;
}
// a page's own name (the strip's name of a top page; the leaf of a hidden one), ASCII
static void LkPageName(int lp, char *out, int n)
{
    char t[LK_TXT * 2];
    out[0] = 0;
    if ((unsigned)lp >= LP_MAX || !LkDecode(LkMsg(g_lpTitle[lp]), t, sizeof t)) {
        sprintf_s(out, n, "Page %d", lp + 1);
        return;
    }
    LkLeaf(t, out, n);
}

// LoadSlot: the title a page shows (hidden pages: "Parent > Page"; MSG_V53 + LK_T0 + lp)
static DWORD LkTitleFor(int lp)
{
    char own[LK_TXT * 2], leaf[LK_TXT], par[LK_TXT], crumb[LK_TXT + 8];
    int parent;
    DWORD id;
    if ((unsigned)lp >= LP_MAX) return 0;
    id = g_lpTitle[lp];
    if (g_lkOn && lp >= LP_N && (parent = LkParentOf(lp)) >= 0) {
        __try {
            if (LkDecode(LkMsg(g_lpTitle[lp]), own, sizeof own)) {
                LkLeaf(own, leaf, sizeof leaf);
                LkPageName(parent, par, sizeof par);
                if (LkCrumb(par, leaf, LK_TITLE_MAX, crumb, sizeof crumb)) {
                    BYTE b[LK_TXT + 8];
                    LkEncode(b, sizeof b, crumb);
                    if ((float)LK_TITLE_X + LkTextW(b, 0, LkSpacing()) <= LK_TITLE_RIGHT) {
                        memcpy(g_lkTitle[lp], b, sizeof b);
                        id = MSG_V53 + LK_T0 + lp;
                    } else {
                        static LONG s_q;
                        if (LogQuota(&s_q, 8)) LogV("options look: page %d: \"%s\" runs into the page arrow: its own title", lp, crumb);
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static LONG s_f;
            if (LogQuota(&s_f, 3)) LogAnomaly("options", "page %d: building its title faulted: its own title", lp);
            id = g_lpTitle[lp];
        }
    }
    g_lkTitleShown[lp] = id;
    return id;
}

// ---------------------------------------------------------------- the dependent rows (layout_data.py DEPS, today's rows)
// Each row is known by its own input function; LkDecorate puts the thunk of its slot in the record (+0) and the matching
// description thunk (+0x18), keeping the row's own description getter.
enum { LKD_NSTR, LKD_NTYPE, LKD_NCOL, LKD_DOFSTR, LKD_ECHON, LKD_WIDTH, LKD_SHEDGE, LKD_SHDARK, LKD_N };
typedef char LkDepsFit[LKD_N <= 8 ? 1 : -1];  // one LK_D0 text each
typedef int(__cdecl *LkInFn)(BYTE *, int);
typedef int(__cdecl *LkDescFn)(void);
static const LkInFn kLkOwnIn[LKD_N] = { FxIn0, FxIn1, FxIn2, DofStrIn, GpInEchoN, SndWidthIn, ShEdgeIn, ShDarkIn };
static const int kLkRule[LKD_N] = { LKR_GRAIN, LKR_GRAIN, LKR_GRAIN, LKR_DOF, LKR_ECHO, LKR_WIDTH, LKR_SHADOW, LKR_SHADOW };
static const char *const kLkRowName[LKD_N] = { OLW_L_GRAIN_STRENGTH, OLW_GRAIN_GRAIN_TYPE_L, OLW_GRAIN_GRAIN_COLOUR_L,
                                               OLW_EFFECTS_BLUR_STRENGTH_L, OLW_POWERS_ECHO_COUNT_L, OLW_SOUND_STEREO_WIDTH_L,
                                               OLW_SHADOWS_EDGE_L, OLW_SHADOWS_DARKNESS_L };  // v3.53 MERGE: w2 OL1's labels (the log)
static DWORD g_lkOwnDesc[LKD_N];  // each row's own description getter (from its record)
static LONG g_lkInertQ[LKD_N];

static int LkRuleOn(int rule)  // 1 = the switch the rule names is on (the row is live)
{
    switch (rule) {
    case LKR_GRAIN: return *(volatile BYTE *)0x1091AE4 != 0;  // cfg+0x14 Noise Effect (0 Off; FxPreviewNoise)
    case LKR_DOF: return g_dofMode != DOF_OFF;
    case LKR_ECHO: return g_gp[GP_ECHO] != 0;
    case LKR_WIDTH: return SndModeNow() != SND_MODE_MONO;  // Stereo, Headphones, Surround (DEPS)
    case LKR_SHADOW: return *(volatile BYTE *)0x1091AE9 != 0;  // cfg+0x19 Realtime Shadow (0 Off; stock input 0x524F20 toggles it)
    case LKR_CAM: return atof(g_camTxt[CAMK_ENABLE]) != 0.0;
    case LKR_MARGIN: return atof(g_camTxt[CAMK_WALLZOOM]) != 0.0;
    }
    return 1;
}
static int LkDepIn(int k, BYTE *cfg, int count)
{
    if (g_lkOn && !LkRuleOn(kLkRule[k])) {
        if ((((Fn2)0x4F84A0)(0, 0x11) || (((int(__cdecl *)(int))0x5542C0)(0) & 0xC)) && LogQuota(&g_lkInertQ[k], 3))  // (as the rows' own inputs read them)
            LogV("options look: %s is grey (%s): the key did nothing", kLkRowName[k], kLkReason[kLkRule[k]]);
        return 0;
    }
    return kLkOwnIn[k](cfg, count);
}
// the reason, then the row's own line: one line of at most 52 characters (v3.53 MERGE: LkComposeLine; the own line whole,
// else its first sentences that fit, else the reason alone; a line that is not plain byte-mode text: the reason alone) that
// also fits descfit's one drawn line (DESC_RIGHT - DESC_X = 416 px in font 1): fewer characters while it is wider
static int LkComposeId(int k, int rule, int own)
{
    char o[LK_TXT * 4], d[LK_TXT];
    int mx = LK_DESC_MAX, n, tries;
    o[0] = 0;
    __try {
        if (own) LkDecode(LkMsg((DWORD)own), o, sizeof o);
        for (tries = 0; tries < 8; ++tries) {  // (each pass drops at least the last sentence; the reason alone ends it)
            n = LkComposeLine(kLkReason[rule], o, mx, d, sizeof d);
            if (!LkEncode(g_lkDesc[k], (int)sizeof g_lkDesc[k], d)) break;
            if (LkTextW(g_lkDesc[k], 1, LkSpacing()) <= DESC_RIGHT - DESC_X || n <= (int)strlen(kLkReason[rule]))
                return MSG_V53 + LK_D0 + k;
            mx = n - 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return MSG_V53 + LK_R0 + rule;
}
static int LkDepDesc(int k)
{
    const DWORD own = g_lkOwnDesc[k];
    int id = own ? ((LkDescFn)(ULONG_PTR)own)() : 0;
    if (!g_lkOn || LkRuleOn(kLkRule[k])) return id;
    if (k == LKD_ECHON) id = MSG_GP0 + GPM_D0 + GP_ECHON;  // its own line (GPM_ECD3 says "turn Echo Doubles on first" again)
    return LkComposeId(k, kLkRule[k], id);
}
#define LK_THUNK(k) \
    static int __cdecl LkIn##k(BYTE *c, int n) { return LkDepIn(k, c, n); } \
    static int __cdecl LkDs##k(void) { return LkDepDesc(k); }
LK_THUNK(0) LK_THUNK(1) LK_THUNK(2) LK_THUNK(3) LK_THUNK(4) LK_THUNK(5) LK_THUNK(6) LK_THUNK(7)
static const LkInFn kLkThunkIn[LKD_N] = { LkIn0, LkIn1, LkIn2, LkIn3, LkIn4, LkIn5, LkIn6, LkIn7 };
static const LkDescFn kLkThunkDesc[LKD_N] = { LkDs0, LkDs1, LkDs2, LkDs3, LkDs4, LkDs5, LkDs6, LkDs7 };

static int LkDepOfRow(int page, int row)  // -1 = not a dependent row (or not decorated)
{
    const DWORD *r;
    int k;
    if ((unsigned)page >= LP_MAX || !g_lpTable[page] || (unsigned)row >= (unsigned)g_lpCount[page]) return -1;
    r = g_lpTable[page] + 7 * row;
    for (k = 0; k < LKD_N; ++k)
        if (r[0] == (DWORD)(ULONG_PTR)kLkThunkIn[k]) return k;
    return -1;
}
// RowLabelC / RowValueC: the row being drawn is a dependent row whose switch is off
static int LkRowGrey(int slot, int row)
{
    int k;
    if (!g_lkOn || (unsigned)slot >= 3) return 0;
    k = LkDepOfRow(g_slotPage[slot], row);
    return k >= 0 && !LkRuleOn(kLkRule[k]);
}
// extramenu.inc: the pause Extra Menu > Camera page (CAMR_*): -1 = live, else the reason (LKR_CAM / LKR_MARGIN)
static int LkCamRowGrey(int r)
{
    if (!g_lkOn || !CamPauseOn() || r <= CAMR_ENABLE || r >= CAMR_N) return -1;
    if (!LkRuleOn(LKR_CAM)) return LKR_CAM;
    if (r == CAMR_MARGIN && !LkRuleOn(LKR_MARGIN)) return LKR_MARGIN;
    return -1;
}
static const BYTE *LkCamReason(int rule) { return (unsigned)rule < LKR_N ? g_lkReason[rule] : g_lkEmpty; }

// ---------------------------------------------------------------- the records of a page (LoadSlot, ResetPages)
static int LkStockTable(const DWORD *t) { return (ULONG_PTR)t >= 0x654000 && (ULONG_PTR)t < 0x656000; }  // the exe's row tables (0x654CC0 / D88 / E50)
static void LkDecorate(int lp)
{
    DWORD *t;
    int n, i, k, folders = 0, deps = 0;
    if (!g_lkOn || (unsigned)lp >= LP_MAX) return;
    t = g_lpTable[lp];
    n = g_lpCount[lp];
    if (!t || n <= 0 || n > 40 || LkStockTable(t)) return;  // the stock table: never written ("Press" stays, no triangle)
    for (i = 0; i < n; ++i) {
        DWORD *r = t + 7 * i;
        if (r[1] == 0x51EB10 && SubmenuRowPage(lp, i)) {  // a folder: no "Press" under its triangle
            if (r[3] != (DWORD)(ULONG_PTR)g_lkNoVal) r[3] = (DWORD)(ULONG_PTR)g_lkNoVal;
            ++folders;
        } else if (r[3] == (DWORD)(ULONG_PTR)g_lkNoVal) {
            r[3] = 0x654C54;  // no longer a folder: the stock {"Press"} again
        }
        for (k = 0; k < LKD_N; ++k) {
            if (r[0] == (DWORD)(ULONG_PTR)kLkOwnIn[k]) {  // (a rebuilt record has its own functions again)
                g_lkOwnDesc[k] = r[6] == (DWORD)(ULONG_PTR)kLkThunkDesc[k] ? g_lkOwnDesc[k] : r[6];
                r[0] = (DWORD)(ULONG_PTR)kLkThunkIn[k];
                r[6] = (DWORD)(ULONG_PTR)kLkThunkDesc[k];
            }
            if (r[0] == (DWORD)(ULONG_PTR)kLkThunkIn[k]) ++deps;
        }
    }
    {
        static int s_seen[24];
        if (!s_seen[lp] && (folders || deps)) {
            s_seen[lp] = 1;
            LogV("options look: page %d: %d folder row(s) with the triangle, %d row(s) greyed while their switch is off", lp, folders, deps);
        }
    }
}

// ---------------------------------------------------------------- drawing (RowChangedDraw: before each row's draw)
typedef int(__cdecl *LkTextFn)(int, int, int, int, int, int);
static void LkStripNames(void)
{
    int p;
    for (p = 0; p < LP_N; ++p) {
        char a[LK_TXT];
        LkPageName(p, a, sizeof a);
        LkEncode(g_lkStrip[p], LK_TXT, a);
    }
    g_lkStripOk = 1;
}
static void LkStripDraw(int lp, int xoff, int rest)  // v3.55 OL56: + rest (the page shown, not sliding: keep the click's boxes)
{
    const int top = LkTopOf(lp);
    const DWORD c00 = *(volatile DWORD *)0x1261C00, c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    const float sp = LkSpacing();
    float w[LP_N], gap, sp2, total, x;
    int adv[LP_N], p, cellH;
    if (!g_lkStripOk) LkStripNames();
    for (p = 0; p < LP_N; ++p) {
        char a[LK_TXT];
        w[p] = LkTextW(g_lkStrip[p], 1, sp);
        adv[p] = LkDecode(g_lkStrip[p], a, sizeof a) ? (int)strlen(a) : 8;
    }
    if (!LkStripFit(w, adv, LP_N, sp, LK_STRIP_ROOM, 6.0f, 22.0f, &gap, &sp2, &total)) {
        static LONG s_q;
        if (LogQuota(&s_q, 1)) LogAnomaly("options", "the page strip is %.0f px wide at the tightest spacing (room %.0f): drawn centred", total, LK_STRIP_ROOM);
    }
    x = 256.0f - total * 0.5f + (float)xoff;
    if (x + total < -16.0f || x > 528.0f) return;  // slid off the canvas
    cellH = *(const short *)(ULONG_PTR)(0x6999B6 + 4 * 1);
    if (cellH < 8 || cellH > 48) cellH = 22;
    if (rest) {  // v3.55 OL56: the boxes (the strip's top to its underline, in the cursor's 448-high space)
        g_lkHitY = (float)LK_STRIP_Y;
        g_lkHitH = cellH * (448.0f / 480.0f) + 3.0f;
        g_lkHitLp = lp;
    }
    ((Fn1)0x556A40)(1);
    if (sp2 != sp) *(volatile float *)0x1261C20 = sp2;
    for (p = 0; p < LP_N; ++p) {
        const float wp = LkWidthAt(w[p], adv[p], sp, sp2);
        const DWORD col = p == lp ? LK_COL_ON : p == top ? LK_COL_TOP : LK_COL_OFF;
        if (rest) g_lkHitX[p] = x, g_lkHitW[p] = wp;  // v3.55 OL56
        ((Fn1)0x556A50)(8);
        ((LkTextFn)0x50B5B0)(-1, (int)(ULONG_PTR)g_lkStrip[p], (int)x + 2, LK_STRIP_Y + 2, 0, 0);
        ((Fn1)0x556A90)((int)col);
        ((LkTextFn)0x50B5B0)(-1, (int)(ULONG_PTR)g_lkStrip[p], (int)x, LK_STRIP_Y, 0, 0);
        if (p == lp && *(volatile int *)0x1261BF8 < 0x3E) {  // the rect queue holds 0x40 (RE 5.1; subtitles.inc)
            DWORD a = *(volatile DWORD *)0x1261C1C & 0xFF;
            if (a > 0x80) a = 0x80;
            *(volatile DWORD *)0x1261C14 = *(volatile DWORD *)0x1261C18 = a << 24 | LK_COL_LINE;
            ((void(__cdecl *)(int, int, int, int))0x557300)((int)x, LK_STRIP_Y + (int)(cellH * (448.0f / 480.0f)), (int)(wp + 0.5f), 2);
        }
        x += wp + gap;
    }
    if (sp2 != sp) *(volatile float *)0x1261C20 = sp;
    *(volatile DWORD *)0x1261C00 = c00;
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
}
static void LkGlyphNote(int measured);
static void LkRowDraw(int row, int slot, const BYTE *obj, int xoff)
{
    int page;
    if (!g_lkOn || (unsigned)slot >= 3 || !obj) return;
    page = g_slotPage[slot];
    if ((unsigned)page >= LP_MAX || !g_lpTable[page] || (unsigned)row >= (unsigned)g_lpCount[page]) return;
    __try {
        if (row == 0) {
            LkStripDraw(page, xoff, xoff == 0 && slot == *(volatile int *)0x1091B1C && page == g_lpage);  // v3.55 OL56: + at rest
            if (!g_descOn && slot == *(volatile int *)0x1091B1C) LkGlyphNote(-1);  // (no description hook: the estimate only)
        }
        if (g_lpTable[page][7 * row + 3] == (DWORD)(ULONG_PTR)g_lkNoVal && SubmenuRowPage(page, row)) {
            RowRect r;
            const int cur = *(const int *)(obj + 0x38 * slot + 0x30);
            const float x = LK_FOLDER_X + (float)xoff;
            memset(&r, 0, sizeof r);
            ((void *(__cdecl *)(RowRect *, const void *, int))0x552270)(&r, obj + 0x38 * slot, row);
            if (r.w > 0 && x > -16.0f && x < 528.0f)
                ((void(__cdecl *)(float, float, int, float, DWORD))0x523620)(x, (float)r.y + 4.0f, 3, 1.0f,
                    0x80000000u | (row == cur ? OPT_SUB_HOT : OPT_SUB_RGB));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static LONG s_f;
        if (LogQuota(&s_f, 3)) LogAnomaly("options", "drawing the page strip / a folder triangle faulted (page %d row %d)", page, row);
    }
}

// ---------------------------------------------------------------- the glyph budget (one line per page)
static int LkPageGlyphs(int lp, int *title, int *strip, int *rows, int *desc, int *flash)
{
    const DWORD *t = g_lpTable[lp];
    const int n = g_lpCount[lp], cur = ActiveCursor();
    int i, p;
    *title = *strip = *rows = *desc = *flash = 0;
    *title = LkGlyphs(LkMsg(g_lkTitleShown[lp] ? g_lkTitleShown[lp] : g_lpTitle[lp]));
    if (g_lkStripOk)
        for (p = 0; p < LP_N; ++p) *strip += LkGlyphs(g_lkStrip[p]);
    for (i = 0; i < n && i < 40; ++i) {
        const DWORD *r = t + 7 * i;
        const DWORD *ids = (const DWORD *)(ULONG_PTR)r[3];
        const int lab = LkGlyphs(LkMsg(r[5]));
        *rows += lab;
        if (i == cur) *flash = 2 * lab;  // the arrival flash draws the label twice more, with its shadow
        if (VbBarRule(r[0], r[5]) || !r[1] || !ids) continue;  // the volume bars draw no text  // v3.55 OL56: by input too (volbar_sim.h)
        {
            const int v = ((int(__cdecl *)(void))(ULONG_PTR)r[1])();
            if (v >= 0 && v < (int)r[4]) *rows += LkGlyphs(LkMsg(ids[v]));
            else if (v == -1 && i == cur && ids[0]) *rows += LkGlyphs(LkMsg(ids[0]));  // "Press"
        }
        if (i == cur && r[6]) *desc = LkGlyphs(LkMsg((DWORD)((int(__cdecl *)(void))(ULONG_PTR)r[6])()));
    }
    *desc += RzSeedGlyphs(lp);  // v3.56 RI356: the Randomizer page's seed line under the description (randomizer.inc)
    return 2 * (*title + *strip + *rows + *desc) + *flash;
}
// descfit.inc DescText (after the description: the queue holds everything the page drew), or the row-0 draw without it
static void LkGlyphNote(int measured)
{
    const int lp = g_lpage;
    if (!g_lkOn || (unsigned)lp >= LP_MAX || !g_lpTable[lp]) return;
    if (lp != g_lkLastLp || g_pending >= 0) {
        g_lkLastLp = lp, g_lkStable = 0;
        return;
    }
    if (++g_lkStable != 3) return;  // settled (no slide), once per visit
    __try {
        int ti, st, ro, de, fl;
        const int est = LkPageGlyphs(lp, &ti, &st, &ro, &de, &fl);
        static LONG s_q, s_a;
        char name[LK_TXT * 2];
        if (est <= g_lkLogged[lp] + 15 && (measured < 0 || measured < 790)) return;  // logged already (it did not grow)
        g_lkLogged[lp] = est > g_lkLogged[lp] ? est : g_lkLogged[lp];
        if (!LkDecode(LkMsg(g_lkTitleShown[lp] ? g_lkTitleShown[lp] : g_lpTitle[lp]), name, sizeof name)) strcpy_s(name, sizeof name, "?");
        if (LogQuota(&s_q, 120))
            LogV("options: page %d (%s): about %d menu glyphs (title %d, page strip %d, rows %d, description %d, each twice with its "
                 "shadow; the cursor's flash %d); measured %d of the 800 the text queue holds", lp, name, est, ti, st, ro, de, fl, measured);
        if ((est > 800 || measured >= 800) && LogQuota(&s_a, 10))
            LogAnomaly("options", "page %d (%s): %d menu glyphs (measured %d): over the 800 the text queue holds, its last rows or "
                       "description may not draw", lp, name, est, measured);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static LONG s_f;
        if (LogQuota(&s_f, 3)) LogAnomaly("options", "counting the glyphs of page %d faulted", lp);
    }
}

// ---------------------------------------------------------------- install / every Options opening
// v3.55 OL56 (R6): the page strip's mouse click. PrevPage (Refined's hook of the generic 0x405570, Menus43) answers 1 for
// the stock Options click test only (called from 0x525B5C: 525AA0 asks it right after 0x405490 saw a click, and a 1 takes the
// page change branch 0x525B7A = hkPageSwitch) when the cursor is on another page's name of the strip shown at rest and that
// page can be reached (LkStripRoute). hkPageSwitch's PageKeyFade then slides there (another slot) or fades there (REQ_JUMP:
// from a hidden page, or to the top page in the same slot). No exe site of its own; only while the strip is drawn (Look=1).
static const char *FcBusy(void);  // v3.56 MERGE: menuclick.inc (MN356): the one mid-slide / fade / waiting-click refusal
static int LkStripClick(DWORD ret)
{
    static LONG s_q;
    float cx = 0.0f, cy = 0.0f;
    int t, go;
    if (ret != 0x525B61 || !g_lkOn || !g_olOn || !g_cursorTramp || g_lkHitLp < 0 || g_lkHitLp != g_lpage) return 0;
    if (FcBusy()) return 0;  // v3.55 OL56-001 (Codex): no click while a slide / fade is pending or a click waits; v3.56 MERGE: the folder
                             // click's own test (g_pending, g_subReq, g_lkStripGo), shared with MN356's menuclick.inc
    __try {
        MenuCursor(&cx, &cy);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    t = LkStripHit(g_lkHitX, g_lkHitW, LP_N, g_lkHitY, g_lkHitH, cx, cy);
    go = LkStripRoute(g_lpage, t, LP_N, g_subScreenOn);
    if (go == LK_GO_NONE) return 0;
    InterlockedExchange(&g_lkStripGo, t);
    if (LogQuota(&s_q, 40)) LogV("options look: the strip: %s clicked on page %d: %s", kOlPage[t].title, g_lpage, go == LK_GO_SLIDE ? "a slide" : "the fade");
    return 1;
}
// PageKeyFade (hkPageSwitch): the clicked page (taken once) and its route; *t = -1 when none
static int LkStripTakeRoute(int *t)
{
    *t = (int)InterlockedExchange(&g_lkStripGo, -1);
    return *t < 0 ? LK_GO_NONE : LkStripRoute(g_lpage, *t, LP_N, g_subScreenOn);
}
static void LkReset(void)  // ResetPages (523E4B): a new Options opening
{
    int p;
    g_lkStripOk = 0;  // the names again (a title may have changed: the Menu Font, a language)
    g_lkHitLp = -1;  // v3.55 OL56: no click before the strip is drawn
    InterlockedExchange(&g_lkStripGo, -1);
    g_lkLastLp = -1, g_lkStable = 0;
    for (p = 0; p < 3; ++p) g_lkTitleShown[p] = g_lpTitle[p];  // the stock pages in their own slots (no LoadSlot)
}
static void LkInstall(void)  // InstallPages, after the pages are in
{
    int r;
    for (r = 0; r < LKR_N; ++r) LkEncode(g_lkReason[r], LK_TXT, kLkReason[r]);
    if (!GetPrivateProfileIntA("Options", "Look", 1, g_iniPath)) {
        Log("options look: off ([Options] Look=0): no page strip, folder triangles, Parent > Page titles or new grey rows");
        return;
    }
    if (!g_pagesOn) return;
    g_lkOn = 1;
    Log("options look: in (the page strip under the title, a blue triangle on folder rows, \"Parent > Page\" titles; grey while "
        "their switch is off: Grain Strength / Type / Colour, Blur Strength, Echo Count, Stereo Width, Shadows' Edge / Darkness, the "
        "pause camera's settings)");  // v3.53 MERGE: w2 OL1's names
}
