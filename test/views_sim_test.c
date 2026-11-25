// v3.41 VW341: views_sim.h native test (staging/v341_feedback/w3_views/PLAN.md, issue 3 of the user's v3.40 QA).
// v3.42 EG342 (staging/v342_feedback/w1_edges/PLAN.md, item 6 of the user's v3.41 QA: the lit column 0 / row 0).
// The backend model: Direct3D 9 screen coordinates (pixel i centred on x = i), the top-left rule (pixel i is in a span
// [x0, x1) when x0 <= i < x1), and every edge of one draw path moved by the backend's offset e (dgVoodoo2's
// Direct3D 11 pixel centres, subpixel snapping, float error): e from -0.49 to +0.49 px, the black-side quads' own e up to
// 0.49 px apart from the picture's.
// 1. The cause: with v3.41's mapping (the game's canvas edges on pixel centres) column 0 and row 0 drop out as soon as
//    e > 0 (any size), the last column / row never: the user's shots. The v3.41 Clear of [0, left) as such a span too.
// 2. v3.42 (VwAlignNdc + VwPillars + VwPillarQuads): at every size from 240 to 2400 lines and 4:3 to 32:9 (plus the
//    named sizes, 480p .. 2160p, the user's 2560x1440 and 2048x1536), for every e: the black sides, the 4:3 picture and
//    the full-canvas fills / grid / backdrops cover every pixel centre of [0, w) x [0, h), column 0 and row 0 included;
//    each pillar hides at most one picture column.
// 3. GOG's wrapper in Borderless: a 4:3 game picture on a 16:9 desktop lands on columns 320..2239 and its Present
//    clears the whole display viewport first, so nothing of the game can reach columns 2558 / 2559 (the v3.40 shots'
//    grey column): what shows there is composed over the window by Windows.
// 4. The edge-cover rule (VwEdgeSliver) on the screenshots' geometry.
// Build and run: test\build_views_test.cmd (cl /O2 x86, as build.cmd).
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/views_sim.h"

static int g_fail, g_n;
static void Check(int ok, const char *what)
{
    ++g_n;
    if (!ok) {
        printf("FAIL  %s\n", what);
        ++g_fail;
    }
}

// The backend's edge offsets (px) of one draw path
static const float kE[] = { -0.49f, -0.25f, -0.125f, -1.0f / 256.0f, 0.0f, 1.0f / 256.0f, 0.125f, 0.25f, 0.49f };
#define NE ((int)(sizeof kE / sizeof kE[0]))

// The pixels a span [x0, x1) of Direct3D 9 screen coordinates covers: [first, end)
static void Span(double x0, double x1, int *first, int *end)
{
    *first = (int)ceil(x0);
    *end = (int)ceil(x1);
}

// The 4:3 picture of Menus43's interface set as the GPU computes it (SetConstants2D: c2' = c2 / S, c3' = c2' * -2048,
// then Apply2D adds the alignment dx), the canvas edges 1792 / 2304 (GS units), the viewport transform, plus e.
static void PictureSpan(int w, int h, int align, float e, int *first, int *end)
{
    const float S = (3.0f * (float)w) / (4.0f * (float)h);
    const float c2 = 0.00390625f / S, c3 = c2 * -2048.0f;
    float dx = 0.0f, dy = 0.0f;
    float cl, cr;
    if (align) VwAlignNdc(w, h, &dx, &dy);
    cl = 1792.0f * c2 + (c3 + dx);
    cr = 2304.0f * c2 + (c3 + dx);
    Span((double)((cl + 1.0f) * 0.5f * (float)w) + e, (double)((cr + 1.0f) * 0.5f * (float)w) + e, first, end);
}

static void V341Pillars(int w, int h, int *left, int *right)  // v3.41 BlackSides (Clear rects)
{
    const long long num = 3LL * w - 4LL * h;
    *left = *right = 0;
    if (num <= 0) return;
    *left = (int)(num / 6) + 1;
    *right = (int)(num / 6);
}

// maxSide0: the most picture columns one pillar hides at e = 0; maxSide: at any e (the pillars keep at least half a
// pixel over the picture's edge, so a column whose centre lies in that margin is hidden too: at most two)
typedef struct { long sizes, gaps, edge0, edgeEnd, hidden0, maxSide, maxSide0; int gw, gh; } Tally;

// v3.42: the black-side quads (offset eq), the aligned picture (offset ec): gaps between them or at the screen edges
static void Measure342(int w, int h, Tally *t)
{
    int l, r, a, b, n;
    float q[2][4];
    if (!VwPillars(w, h, &l, &r)) return;
    n = VwPillarQuads(w, h, l, r, q);
    if (n != 2) {
        ++t->gaps;
        return;
    }
    for (a = 0; a < NE; ++a)
        for (b = 0; b < NE; ++b) {
            int pf, pe, lf, le, rf, re, hl, hr;
            if (fabs((double)kE[a] - (double)kE[b]) >= 0.5) continue;
            PictureSpan(w, h, 1, kE[a], &pf, &pe);
            Span((double)q[0][0] + kE[b], (double)q[0][2] + kE[b], &lf, &le);
            Span((double)q[1][0] + kE[b], (double)q[1][2] + kE[b], &rf, &re);
            if (lf > 0) ++t->edge0;
            if (re < w) ++t->edgeEnd;
            if (le < pf || pe < rf || lf > 0 || re < w) {
                if (!t->gaps) { t->gw = w; t->gh = h; }
                ++t->gaps;
            }
            hl = le > pf ? le - pf : 0;
            hr = pe > rf ? pe - rf : 0;
            if (kE[a] == 0.0f && kE[b] == 0.0f) t->hidden0 += hl + hr;
            if (hl > t->maxSide) t->maxSide = hl;
            if (hr > t->maxSide) t->maxSide = hr;
            if (kE[a] == 0.0f && kE[b] == 0.0f && hl > t->maxSide0) t->maxSide0 = hl;
            if (kE[a] == 0.0f && kE[b] == 0.0f && hr > t->maxSide0) t->maxSide0 = hr;
            {  // the pillars' rows: one pixel past the top and the bottom
                int yf, ye;
                Span((double)q[0][1] + kE[b], (double)q[0][3] + kE[b], &yf, &ye);
                if (yf > 0 || ye < h) ++t->gaps;
            }
        }
    ++t->sizes;
}

// v3.41: the unaligned picture and the Clear rects drawn as spans [0, left) / [w - right, w) with the same e
static void Measure341(int w, int h, Tally *t)
{
    int l, r, a;
    V341Pillars(w, h, &l, &r);
    if (l <= 0) return;
    for (a = 0; a < NE; ++a) {
        int pf, pe, lf, le, rf, re;
        PictureSpan(w, h, 0, kE[a], &pf, &pe);
        Span(0.0 + kE[a], (double)l + kE[a], &lf, &le);
        Span((double)(w - r) + kE[a], (double)w + kE[a], &rf, &re);
        if (lf > 0) ++t->edge0;
        if (re < w) ++t->edgeEnd;
        if (le < pf || pe < rf || lf > 0 || re < w) {
            if (!t->gaps) { t->gw = w; t->gh = h; }
            ++t->gaps;
        }
    }
    ++t->sizes;
}

static const int kNamed[][2] = {
    { 2560, 1440 }, { 1920, 1080 }, { 3440, 1440 }, { 1280, 720 }, { 784, 588 }, { 1366, 768 }, { 1360, 768 },
    { 1600, 900 }, { 1680, 1050 }, { 1280, 800 }, { 1920, 1200 }, { 2560, 1080 }, { 3840, 2160 }, { 5120, 1440 },
    { 3840, 1080 }, { 2560, 1600 }, { 1281, 720 }, { 1024, 768 }, { 1920, 1440 }, { 7680, 2160 }, { 2048, 1536 },
    { 640, 480 }, { 854, 480 }, { 720, 480 }, { 3200, 1800 }, { 2880, 1800 }, { 1440, 1080 }, { 2880, 2160 },
};
#define NNAMED ((int)(sizeof kNamed / sizeof kNamed[0]))

static void TestPillars(void)
{
    Tally nw = { 0 }, old = { 0 };
    int i, w, h, l, r;
    char msg[240];
    for (i = 0; i < NNAMED; ++i) {
        Tally one = { 0 }, o = { 0 };
        w = kNamed[i][0];
        h = kNamed[i][1];
        if (!VwPillars(w, h, &l, &r)) {
            sprintf(msg, "%dx%d: 4:3 or narrower, no pillars", w, h);
            Check(3 * w <= 4 * h, msg);
            continue;
        }
        Measure342(w, h, &one);
        sprintf(msg, "%dx%d: pillars %d | %d and the aligned 4:3 picture cover every column, 0 and %d included (e -0.49..+0.49 px)",
                w, h, l, r, w - 1);
        Check(one.gaps == 0 && one.sizes == 1, msg);
        sprintf(msg, "%dx%d: each pillar hides at most one picture column at e = 0 (%ld), two at any e (%ld)", w, h,
                one.maxSide0, one.maxSide);
        Check(one.maxSide0 <= 1 && one.maxSide <= 2, msg);
        Measure341(w, h, &o);
        printf("  %4dx%-4d  v3.42 %d | %d: gaps %ld, hidden %ld   v3.41: column 0 lost at %ld of %d offsets, the last column at %ld\n",
               w, h, l, r, one.gaps, one.hidden0, o.edge0, NE, o.edgeEnd);
    }
    VwPillars(2560, 1440, &l, &r);
    Check(l == 321 && r == 321, "2560x1440: pillars 321 | 321 (the aligned picture covers the pixel area 320..2240; one column hidden each side)");
    VwPillars(3440, 1440, &l, &r);
    Check(l == 761 && r == 761, "3440x1440 (21:9): pillars 761 | 761");
    VwPillars(1280, 800, &l, &r);
    Check(l == 108 && r == 108, "1280x800 (16:10, L = 106.67): pillars 108 | 108");
    VwPillars(1920, 1080, &l, &r);
    Check(l == 241 && r == 241, "1920x1080: pillars 241 | 241");
    Check(!VwPillars(784, 588, &l, &r) && !l && !r, "784x588 (4:3): no pillars");
    Check(!VwPillars(2048, 1536, &l, &r), "2048x1536 (4:3): no pillars");
    Check(!VwPillars(1280, 1024, &l, &r), "1280x1024 (5:4): no pillars");
    Check(!VwPillars(0, 720, &l, &r) && !VwPillars(1280, 0, &l, &r), "a bad size: no pillars");
    {
        float q[2][4];
        VwPillars(2560, 1440, &l, &r);
        Check(VwPillarQuads(2560, 1440, l, r, q) == 2 && q[0][0] < -1.0f && q[0][1] < -1.0f && q[1][2] > 2560.0f &&
                  q[1][3] > 1440.0f && q[0][2] == 320.5f && q[1][0] == 2238.5f,
              "2560x1440: the quads run 1 px past every screen edge; inner edges at 320.5 / 2238.5 (pixel 320 and 2239 hidden)");
        Check(VwPillarQuads(2560, 1440, 0, 0, q) == 0 && VwPillarQuads(0, 1440, 5, 5, q) == 0, "no width or a bad size: no quads");
    }
    for (h = 240; h <= 2400; ++h)
        for (w = (4 * h) / 3 + 1; w <= 4 * h && w <= 7680; ++w) {
            Measure342(w, h, &nw);
            Measure341(w, h, &old);
        }
    printf("sweep: %ld sizes (240..2400 lines, 4:3..32:9; %d backend offsets, quad offsets within 0.49 px of the picture's)\n",
           nw.sizes, NE);
    printf("  v3.42: %ld gap(s); column 0 lost %ld time(s), the last column %ld; %ld picture column(s) hidden at e = 0; "
           "at most %ld by one pillar (%ld at any e)\n", nw.gaps, nw.edge0, nw.edgeEnd, nw.hidden0,
           nw.maxSide0, nw.maxSide);
    printf("  v3.41: %ld gap(s) over %ld sizes x %d offsets; column 0 lost %ld time(s), the last column %ld\n", old.gaps,
           old.sizes, NE, old.edge0, old.edgeEnd);
    Check(nw.sizes == old.sizes && nw.sizes > 7000000, "sweep: every wide size measured by both rules");
    Check(nw.gaps == 0 && nw.edge0 == 0 && nw.edgeEnd == 0, "sweep: the v3.42 sides and picture cover every column at every e");
    Check(nw.maxSide0 <= 1 && nw.maxSide <= 2,
          "sweep: one v3.42 pillar hides at most one picture column at e = 0, two at any e (of 1440 .. 2400 picture columns)");
    Check(old.edge0 == old.sizes * 4 && old.edgeEnd == 0,
          "the cause: v3.41's sides lose column 0 for every e > 0 (4 of the offsets) at every size, the last column never");
}

// A full-canvas draw (fills, cinema bars' ends, the wobbling grid, backdrops, 4:3 pictures at 4:3) in both canvases:
// 512x448 (c2 / c3, GS 1792..2304 x 1824..2272) and 640x480 (c94 / c95). Returns the pixels it misses at the four
// edges (bit 0 column 0, 1 row 0, 2 the last column, 3 the last row).
static int CanvasMiss(int w, int h, int align, int big, float e)
{
    float dx = 0.0f, dy = 0.0f, cl, cr, ct, cb;
    int xf, xe, yf, ye, miss = 0;
    if (align) VwAlignNdc(w, h, &dx, &dy);
    if (!big) {
        const float c2 = 0.00390625f, c3 = c2 * -2048.0f, cy2 = 1.0f * -0.004464286f, cy3 = cy2 * -2048.0f;
        cl = 1792.0f * c2 + (c3 + dx);
        cr = 2304.0f * c2 + (c3 + dx);
        ct = 1824.0f * cy2 + (cy3 + dy);
        cb = 2272.0f * cy2 + (cy3 + dy);
    } else {
        const float c94x = 0.003125f, c94y = -0.004166667f;
        cl = 0.0f * c94x + (-1.0f + dx);
        cr = 640.0f * c94x + (-1.0f + dx);
        ct = 0.0f * c94y + (1.0f + dy);
        cb = 480.0f * c94y + (1.0f + dy);
    }
    Span((double)((cl + 1.0f) * 0.5f * (float)w) + e, (double)((cr + 1.0f) * 0.5f * (float)w) + e, &xf, &xe);
    Span((double)((1.0f - ct) * 0.5f * (float)h) + e, (double)((1.0f - cb) * 0.5f * (float)h) + e, &yf, &ye);
    if (xf > 0) miss |= 1;
    if (yf > 0) miss |= 2;
    if (xe < w) miss |= 4;
    if (ye < h) miss |= 8;
    return miss;
}

static void TestNative(void)
{
    long sizes = 0, bad = 0, oldCol0 = 0, oldRow0 = 0, oldEnd = 0, oldAt0 = 0;
    int w, h, i, a, big;
    char msg[200];
    for (h = 240; h <= 2400; h += 3)
        for (w = 320; w <= 7680; w += 7) {
            ++sizes;
            for (big = 0; big < 2; ++big)
                for (a = 0; a < NE; ++a) {
                    const int m = CanvasMiss(w, h, 1, big, kE[a]), o = CanvasMiss(w, h, 0, big, kE[a]);
                    if (m) ++bad;
                    if (o & 1) ++oldCol0;
                    if (o & 2) ++oldRow0;
                    if (o & 12) ++oldEnd;
                    if (o && kE[a] == 0.0f) ++oldAt0;
                }
        }
    printf("canvases: %ld sizes x 2 canvases x %d offsets: v3.42 misses %ld; v3.41 misses column 0 %ld, row 0 %ld, the "
           "last column / row %ld time(s) (%ld at e = 0)\n", sizes, NE, bad, oldCol0, oldRow0, oldEnd, oldAt0);
    Check(bad == 0, "the aligned canvases (fills, cinema bars, the wobbling grid, backdrops, 4:3 pictures at 4:3) cover column 0, "
                    "row 0 and the last ones at every size and e");
    Check(oldCol0 >= sizes * 2 * 4 && oldRow0 >= sizes * 2 * 4 && oldEnd == 0,
          "the cause: the game's own mapping loses column 0 and row 0 for every e > 0, the last column / row never");
    for (i = 0; i < NNAMED; ++i) {
        int miss = 0;
        for (big = 0; big < 2; ++big)
            for (a = 0; a < NE; ++a) miss |= CanvasMiss(kNamed[i][0], kNamed[i][1], 1, big, kE[a]);
        sprintf(msg, "%dx%d: the aligned canvases cover [0, %d) x [0, %d)", kNamed[i][0], kNamed[i][1], kNamed[i][0], kNamed[i][1]);
        Check(miss == 0, msg);
    }
    {  // the user's 4:3 size: the Options page's top row at 2048x1536 (faint in shot 9)
        Check(CanvasMiss(2048, 1536, 0, 1, 1.0f / 256.0f) == 3 && CanvasMiss(2048, 1536, 1, 1, 1.0f / 256.0f) == 0,
              "2048x1536: v3.41 loses column 0 / row 0 of the 640x480 grid at e = +1/256, v3.42 none");
        Check(CanvasMiss(2560, 1440, 0, 1, 1.0f / 256.0f) == 3 && CanvasMiss(2560, 1440, 1, 1, 1.0f / 256.0f) == 0,
              "2560x1440: v3.41 loses column 0 / row 0 of the 640x480 grid at e = +1/256, v3.42 none");
    }
    {  // the 1:1 screen copy (the 640x480 canvas with a w x h texture): pixel i's centre on texel i's centre after the alignment
        float dx, dy;
        VwAlignNdc(2560, 1440, &dx, &dy);
        Check(fabs(((((-1.0f + dx) + 1.0f) * 0.5f * 2560.0f) + 0.5f)) < 1e-3 && fabs(((1.0f - (1.0f + dy)) * 0.5f * 1440.0f) + 0.5f) < 1e-3,
              "alignment: the canvas origin lands on (-0.5, -0.5), the corner of pixel 0 (as the game's post pass quad)");
    }
}

// GOG's wrapper (ori_d3d8.dll, research/gog_wrapper/report.md): Present 0x19005A80 sets the viewport to the whole display
// (0x19005FAA: 0, 0, display w, h), clears it (0x19006410: Clear(0, NULL, TARGET, 0)), then draws the game picture as one
// quad (ox, oy) - (ox + appW * s, oy + appH * s) - 0.49 (aspect=enabled, scaling=fit: s = min of the two scales).
static void TestWrapper(void)
{
    static const int kApp[][2] = { { 1920, 1440 }, { 2560, 1440 }, { 2304, 1440 }, { 3360, 1440 }, { 1024, 768 }, { 784, 588 } };
    const int dw = 2560, dh = 1440;
    int i;
    char msg[200];
    for (i = 0; i < (int)(sizeof kApp / sizeof kApp[0]); ++i) {
        const int aw = kApp[i][0], ah = kApp[i][1];
        const double sx = (double)dw / aw, sy = (double)dh / ah, s = sx < sy ? sx : sy;
        const double ox = (dw - aw * s) / 2.0, x0 = ox - 0.49, x1 = ox + aw * s - 0.49;
        const int first = (int)ceil(x0), end = (int)ceil(x1);
        sprintf(msg, "wrapper: a %dx%d game on the 2560x1440 desktop draws columns %d..%d; the rest is its black clear", aw, ah, first, end - 1);
        Check(first >= 0 && end <= dw && end - first >= (int)floor(aw * s) - 1, msg);
        if (aw == 1920 && ah == 1440)
            Check(first == 320 && end == 2240 && end <= dw - 2, "wrapper: the 4:3 game picture ends at column 2239: columns 2558 / 2559 are the wrapper's black clear");
    }
}

static void TestCover(void)
{
    const VwRect game = { -2560, 0, 0, 1440 };  // the user's game monitor (Secondary Display at -2560,0)
    VwRect c;
    int e;
    // the screenshots' grey column: 2 px at the right edge from y 15 to 1389 (1125-line shots x 1.28)
    const VwRect bleed = { -2, 15, 1400, 1389 };
    const VwRect taskbarHidden = { -2560, 1438, 0, 1486 };  // an auto-hidden taskbar keeps a 2 px sliver on screen
    const VwRect taskbar = { -2560, 1392, 0, 1440 };        // a shown taskbar: 48 px, not a crack
    const VwRect beside = { 0, 0, 1280, 1392 };            // a window on the main display only
    const VwRect over = { -2000, 200, -800, 1000 };         // a window over the game's picture
    e = VwEdgeSliver(bleed, game, 16, &c);
    Check(e == VW_RIGHT && c.r - c.l == 2 && c.b - c.t == 1374 && c.t == 15, "a window 2 px over the right edge: the right edge, 2x1374 at y 15");
    Check(!strcmp(VwEdgeName(e), "right"), "named right");
    e = VwEdgeSliver(taskbarHidden, game, 16, &c);
    Check(e == VW_BOTTOM && c.r - c.l == 2560 && c.b - c.t == 2, "an auto-hidden taskbar's sliver: the bottom edge, 2560x2");
    Check(VwEdgeSliver(taskbar, game, 16, &c) == 0, "a shown taskbar (48 px) is no thin strip");
    e = VwEdgeSliver(taskbar, game, 128, &c);
    Check(e == VW_BOTTOM && c.b - c.t == 48, "a shown taskbar over the game, at the taskbar's 128 px limit: the bottom edge, 2560x48");
    VwEdgeSliver(beside, game, 0, &c);
    Check(c.r <= c.l, "the intersection of a window on the other monitor is empty (the loop's first test)");
    Check(VwEdgeSliver(beside, game, 16, &c) == 0, "a window on the other monitor only: nothing");
    Check(VwEdgeSliver(over, game, 16, &c) == 0, "a window over the picture: not an edge sliver");
    {
        const VwRect corner = { -3, -3, 3, 3 };  // a 3x3 corner
        e = VwEdgeSliver(corner, game, 16, &c);
        Check((e & (VW_RIGHT | VW_TOP)) == (VW_RIGHT | VW_TOP), "a corner sliver touches two edges");
    }
}

int main(void)
{
    TestPillars();
    TestNative();
    TestWrapper();
    TestCover();
    printf("views_sim: %d check(s), %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
