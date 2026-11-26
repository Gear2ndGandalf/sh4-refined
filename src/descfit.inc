// ---------------------------------------------------------------- v3.32 DESC332: Options descriptions fit at every aspect
// User (v3.31 feedback H): "At 4:3 Aspect Ratio some of our cheat descriptions get cut off in our options menu."
// The description of the row under the cursor is drawn by 0x524160 (font 1, shadow `call 50B580` at 0x52418D, the
// text `call 50B580` at 0x52419E) at canvas x 64, y 360 (its caller 0x526653). The 512-wide canvas is the whole 4:3
// screen, so a line wider than 448 px ran off its right edge (in wide shapes Menus43 keeps the canvas in the middle and
// the overflow stayed visible in the pillar: the cut showed at 4:3 only). Both calls now go through DescDraw, which
// measures the string with the game's own font metrics and, when it is wider than DESC_RIGHT - DESC_X, draws a wrapped
// copy (bank -1 = a string pointer), a slightly tighter letter spacing, or a shortened copy (descfit_sim.h). A string
// that fits draws exactly as before (same id, same position, same spacing): v3.31 behaviour.
#include "descfit_sim.h"

#define DESC_X 64.0f       // the description's x at rest (0x526653: (48 - 512) * 1 + 512 + 16)
#define DESC_Y 360.0f      // its y (0x52664E `mov edi, 168h`)
#define DESC_RIGHT 480.0f  // a line ends here at most: 32 px inside the 4:3 edge (shadow +2, the outline ring, a wrapper's edge)
static int g_descOn;       // both calls are ours (all or nothing; the shadow hook passes through until then)
static BYTE g_descBuf[2][DF_MAXBYTES + 16];  // the drawn copies: 0 the text, 1 the shadow
static int g_descPage = -1;                  // the page of the last description drawn and its box (screenfx.inc's panel)
static float g_descTop = DESC_Y, g_descBottom = DESC_Y + 24.0f, g_descRight = DESC_RIGHT;
static float g_descLineH = 24.0f;

// The lowest row's text bottom on the page shown: LoadSlot's rows (96 + step * i, SlotRowStep) and font 0's 30 px line.
static float DescRowsBottom(void)
{
    int n = 7;
    if (g_pagesOn && (unsigned)g_lpage < LP_MAX && g_lpCount[g_lpage] > 0) {
        n = g_lpCount[g_lpage];
    } else {
        const int p = *(volatile int *)0x1091B1C;
        if ((unsigned)p < 3) n = *(const int *)(ULONG_PTR)(0x655040 + 4 * p);
    }
    if (n < 1 || n > 32) n = 7;
    return 96.0f + (float)(SlotRowStep(n) * (n - 1)) + 30.0f;
}
// Nothing of the description below this: the canvas bottom, or on the Subtitle Style page the top of its preview line
// (subtitles.inc SubPreview: font 0, bottom at y 424, up to 1.3 x 30 px with the Huge size, and its box).
static float DescFloor(void)
{
    if (g_lpage == LP_SUBT && g_subOn) return 376.0f;  // v3.54 OL34: Interface > Subtitles
    return 444.0f;
}

static float DescSpacingNow(void)
{
    const float sp = *(volatile float *)0x1261C20;
    return sp >= 0.0f && sp < 20.0f ? sp : 2.5f;
}

// The plan for the string of (bank, id) in the font and spacing set now (0x524160 has set font 1). NULL: draw the source.
static const BYTE *DescPlanFor(int bank, int id, int which, DfPlan *p)
{
    const int fid = *(volatile int *)0x1261C00;
    const BYTE *s = ((const BYTE *(__cdecl *)(int, int))0x50C440)(bank, id);
    DfFont f;
    DfBox b;
    float lh;
    f.fontId = (unsigned)fid < 4 ? fid : 1;
    f.font = *(const BYTE *const *)(ULONG_PTR)(0x1261B98 + 4 * f.fontId);
    f.cellW = (float)*(const short *)(ULONG_PTR)(0x6999B4 + 4 * f.fontId);
    if (f.cellW < 8.0f || f.cellW > 64.0f) f.cellW = 20.0f;
    lh = (float)*(const short *)(ULONG_PTR)(0x6999B6 + 4 * f.fontId) + *(volatile float *)0x1261C24;
    if (!(lh >= 16.0f && lh <= 64.0f)) lh = 26.0f;
    b.maxW = DESC_RIGHT - DESC_X;
    b.spacing = DescSpacingNow();
    b.minSpacing = b.spacing < 1.0f ? b.spacing : 1.0f;
    b.y = DESC_Y;
    b.rowsBottom = DescRowsBottom();
    b.floorY = DescFloor();
    b.lineH = lh * (448.0f / 480.0f);  // the layout's line pitch is in 480-high units
    b.gap = 4.0f;
    g_descLineH = b.lineH;
    DescFit(s, DF_MAXBYTES, &f, &b, g_descBuf[which], (int)sizeof g_descBuf[which], p);
    return p->outLen > 0 ? g_descBuf[which] : NULL;
}

static int __cdecl OutlineShadow(int bank, int id, int x, int y);  // (above: the Screen Effects text ring)
static int DescDraw(int bank, int id, int x, int y, int shadow)
{
    typedef int(__cdecl *Draw_t)(int, int, int, int);
    const Draw_t draw = shadow ? (Draw_t)OutlineShadow : (Draw_t)0x50B580;
    const float sp0 = DescSpacingNow();
    const BYTE *alt = NULL;
    DfPlan p;
    int r, dy;
    p.mode = DF_PASS;
    if (!g_descOn || bank < 0) return draw(bank, id, x, y);
    __try {
        alt = DescPlanFor(bank, id, shadow, &p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) Log("options: a description measure faulted (logged once): drawn as it is");
        p.mode = DF_PASS, alt = NULL;
    }
    if (p.mode <= DF_SAME) {
        if (!shadow) g_descPage = g_lpage, g_descTop = DESC_Y, g_descBottom = DESC_Y + g_descLineH, g_descRight = DESC_X + p.wAfter;
        return draw(bank, id, x, y);
    }
    dy = (int)floor(p.dy + 0.5f);
    if (p.spacing != sp0) *(volatile float *)0x1261C20 = p.spacing;
    r = alt ? draw(-1, (int)(ULONG_PTR)alt, x, y + dy) : draw(bank, id, x, y + dy);
    if (p.spacing != sp0) *(volatile float *)0x1261C20 = sp0;
    if (!shadow) {
        static DWORD s_seen[48];
        static int s_n;
        int i;
        g_descPage = g_lpage;
        g_descTop = DESC_Y + (float)dy;
        g_descBottom = g_descTop + (float)p.lines * g_descLineH;
        g_descRight = DESC_X + p.wAfter;
        for (i = 0; i < s_n && s_seen[i] != (DWORD)id; ++i) {}
        if (i == s_n && s_n < 48) {  // each fitted description once in the log
            static const char *const kMode[4] = { "as it was", "wrapped", "tighter spacing", "shortened" };
            s_seen[s_n++] = (DWORD)id;
            Log("options: description %04X is %.0f px wide (room %.0f): %s, %d line(s), spacing %.1f, widest %.0f px, y %+d",
                id, p.wBefore, DESC_RIGHT - DESC_X, kMode[p.mode], p.lines, p.spacing, p.wAfter, dy);
        }
    }
    return r;
}
static int __cdecl DescShadow(int bank, int id, int x, int y) { return DescDraw(bank, id, x, y, 1); }  // 0x52418D
static int __cdecl DescText(int bank, int id, int x, int y)  // 0x52419E
{
    const int r = DescDraw(bank, id, x, y, 0);
    LkGlyphNote(*(volatile int *)0x1261BF4);  // v3.53 OL2: the page's glyphs, the queue measured after its description
    return r;
}

// screenfx.inc's dark panel behind the description (Screen Effects page): grown to the box drawn on that page
static void DescPanelBox(int lp, float *x1, float *y0, float *y1)
{
    if (!g_descOn || g_descPage != lp) return;
    if (g_descTop - 8.0f < *y0) *y0 = g_descTop - 8.0f;
    if (g_descBottom + 8.0f > *y1) *y1 = g_descBottom + 8.0f;
    if (g_descRight + 8.0f > *x1) *x1 = g_descRight + 8.0f;
}

// Both description calls of 0x524160 stock (the shadow one may already be OutlineShadow's): 1 = installable.
static const Site kDescText = { 0x52419E, { 0xE8, 0xDD, 0x73, 0xFE, 0xFF }, 5 };  // call 50B580 (the text)
static int DescSitesOk(void)
{
    return SiteOk(&kDescText);
}
// After the shadow site was pointed at DescShadow (which passes through while g_descOn is 0): the text site. Off: stock.
static void DescInstall(int sitesOk, int shadowOurs)
{
    if (!sitesOk || !shadowOurs) {
        Log("options: descriptions keep their stock draw (%s differs from the GOG v7 exe)", sitesOk ? "0052418D" : "0052419E");
        return;
    }
    if (!GetPrivateProfileIntA("Window", "DescFit", 1, g_iniPath)) {  // v3.32 REV332-002 (inspection 2): hidden off switch
        Log("options: descriptions keep their stock draw ([Window] DescFit=0)");
        return;  // the shadow hook passes through while g_descOn is 0: both calls stock
    }
    if (!SiteOk(&kDescText) || !PokeBranch(kDescText.va, 0xE8, DescText, 5)) {
        Log("options: descriptions keep their stock draw (0052419E differs from the GOG v7 exe)");
        return;
    }
    g_descOn = 1;
    Log("options: descriptions fit the 4:3 canvas (0052418D / 0052419E: wrapped, else tighter, else shortened; right edge %.0f)",
        DESC_RIGHT);
}
