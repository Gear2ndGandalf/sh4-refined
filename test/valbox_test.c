// v3.58 MN358: the native test of src\valbox_sim.h (the Options value box; test\build_valbox_test.cmd).
// Checks, for synthetic pages and for every page of the plan (ol_words.h: its value words, its row names as the labels,
// measured with two width models: the old flat 10.4 px a character and a wide 13 px one):
//   - the arrows' x is ONE value per page: for every row and every value of it (the user's "arrows stay aligned"), at
//     rest and at every slide offset, through the very arithmetic the hooks use (col = X0 - move; left = col - 20; right
//     = col + 100 + shift; the bar rows' stubs: ebp - move - 20 / ebp - move + shift + 100);
//   - the stock places (344 / 464) when the page's values fit the stock box; the right arrow never past x 500 (ends by
//     508); the left arrow never into the page's widest label; every value centred with VBX_PAD of air when it fits;
//   - the mouse rects: contiguous, each arrow inside its own rect, and a click at the drawn arrow's middle hits its rect
//     at 4:3 / 16:9 / 21:9 / 32:9 and several window sizes (folderclick_sim.h's Menus43 mapping), ten clicks in a row
//     through every value of every row: never a miss.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../src/valbox_sim.h"
#include "../src/folderclick_sim.h"
#include "../src/ol_words.h"

static int g_n, g_fail;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; if (g_fail < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

#define MAXR 16
#define MAXV 32
typedef struct {
    const char *name;
    int rows;
    int nv[MAXR];
    float w[MAXR][MAXV];
    float label[MAXR];
} Page;

static float WidestOf(const Page *p, float *lab)
{
    float m = 0.0f, l = 0.0f;
    int r, v;
    for (r = 0; r < p->rows; ++r) {
        if (p->label[r] > l) l = p->label[r];
        for (v = 0; v < p->nv[r]; ++v)
            if (p->w[r][v] > m) m = p->w[r][v];
    }
    *lab = l;
    return m;
}

static int g_clicks, g_pages, g_moved;

// every check on one page
static void CheckPage(const Page *p)
{
    static const float kAsp[4] = { 4.0f / 3.0f, 16.0f / 9.0f, 21.0f / 9.0f, 32.0f / 9.0f };
    static const int kH[3] = { 720, 1080, 1440 };
    static const int kSlide[5] = { 0, -40, 40, -512, 512 };
    VbxBox b;
    float lab, wv = WidestOf(p, &lab);
    int r, v, k, la = 0, ra = 0, first = 1;
    VbxMake(&b, wv, lab);
    ++g_pages;
    if (b.move || b.shift) ++g_moved;
    CHECK(b.r <= VBX_R_MAX && VBX_X0 + b.r + VBX_ARROW_W <= 508, "%s: the right arrow ends by 508 (%d)", p->name, VBX_X0 + b.r + VBX_ARROW_W);
    CHECK(b.r - b.l >= VBX_SPAN0, "%s: never narrower than the stock box (%d)", p->name, b.r - b.l);
    if (wv + 2 * VBX_PAD <= (float)VBX_SPAN0)
        CHECK(VbxLeftArrowX(&b, VBX_X0) == 344 && VbxRightArrowX(&b, VBX_X0) == 464 && !b.move && !b.shift,
              "%s: values up to %.1f px keep the stock arrows (%d / %d)", p->name, wv, VbxLeftArrowX(&b, VBX_X0), VbxRightArrowX(&b, VBX_X0));
    CHECK(VbxLeftArrowX(&b, VBX_X0) >= (int)ceilf(VBX_LABEL_X + lab) + VBX_LABEL_GAP || b.l == VBX_L0,
          "%s: the left arrow (%d) clear of the widest label (ends %.1f)", p->name, VbxLeftArrowX(&b, VBX_X0), VBX_LABEL_X + lab);
    for (k = 0; k < 5; ++k) {
        const int x0 = VBX_X0 + kSlide[k];
        first = 1;
        for (r = 0; r < p->rows; ++r)
            for (v = 0; v < p->nv[r]; ++v) {
                const int col = x0 - b.move;                      // 0x52570E: ebp -= move
                const int left = col - 20;                        // 0x5257C3: [ebp - 14h]
                const int right = col + VBX_R0 + b.shift;         // 0x52587C (+ shift), 0x52592B (+ 64h)
                const int barL = (x0 - (int)b.move) - 20;         // 0x5256B6 stub (sub ebp, move), 0x5256C0
                const int barR = (x0 - (int)b.move) + (int)b.shift + 100;  // 0x5256E2 stub (add ebp, shift), 0x5256EC
                const int tx = VbxTextX(&b, col, p->w[r][v]);
                const float gl = (float)tx - (float)(left + VBX_ARROW_W), gr = (float)right - ((float)tx + p->w[r][v]);
                if (first) la = left, ra = right, first = 0;
                CHECK(left == la && right == ra, "%s slide %d row %d value %d: the arrows moved (%d / %d, were %d / %d)", p->name,
                      kSlide[k], r, v, left, right, la, ra);
                CHECK(left == VbxLeftArrowX(&b, x0) && right == VbxRightArrowX(&b, x0), "%s: the hooks' arithmetic = the box", p->name);
                CHECK(barL == left && barR == right, "%s: the volume bars' arrows line up (%d / %d vs %d / %d)", p->name, barL, barR, left, right);
                CHECK(fabsf(gl - gr) <= 1.01f, "%s row %d value %d (%.1f px): centred (air %.1f / %.1f)", p->name, r, v, p->w[r][v], gl, gr);
                if (b.fits) CHECK(gl >= VBX_PAD - 0.51f && gr >= VBX_PAD - 0.51f, "%s row %d value %d: %.1f px with air %.1f / %.1f",
                                  p->name, r, v, p->w[r][v], gl, gr);
            }
    }
    {   // the mouse: rects where the arrows are, contiguous; a click on a drawn arrow hits it at every shape and size
        int L[2], V[2], R[2], dl, dv, dvw, dr, a, h;
        VbxRects(&b, VBX_X0, L, V, R);
        VbxRectDeltas(&b, &dl, &dv, &dvw, &dr);
        CHECK(L[0] == 344 + dl && V[0] == 364 + dv && V[1] == 100 + dvw && R[0] == 464 + dr && L[1] == 20 && R[1] == 20,
              "%s: the stub's deltas make the box's rects", p->name);
        CHECK(L[0] + L[1] == V[0] && V[0] + V[1] == R[0], "%s: the rects are contiguous", p->name);
        CHECK(L[0] <= VbxLeftArrowX(&b, VBX_X0) && VbxLeftArrowX(&b, VBX_X0) + VBX_ARROW_W <= L[0] + L[1], "%s: the left arrow in its rect", p->name);
        CHECK(R[0] <= VbxRightArrowX(&b, VBX_X0) && VbxRightArrowX(&b, VBX_X0) + VBX_ARROW_W <= R[0] + R[1], "%s: the right arrow in its rect", p->name);
        for (a = 0; a < 4; ++a)
            for (h = 0; h < 3; ++h) {
                const int ch = kH[h], cw = (int)(ch * kAsp[a] + 0.5f);
                const float s = kAsp[a] / (4.0f / 3.0f);
                const int y = 96 + 26 * 3;  // a row's rect (pitch 26), the arrow 4 below its top, 24 tall
                const int rl[4] = { L[0], y, L[1], 26 }, rr[4] = { R[0], y, R[1], 26 };
                int click;
                for (click = 0; click < 10; ++click)
                    for (r = 0; r < p->rows; ++r)
                        for (v = 0; v < p->nv[r]; ++v) {
                            // the arrow does not move with the value: the pointer the user left there stays on it
                            int mx, my;
                            FcDrawnAt((float)VbxLeftArrowX(&b, VBX_X0) + 4.0f, (float)y + 4.0f + 12.0f, s, cw, ch, &mx, &my);
                            CHECK(FcHitClient(rl, s, cw, ch, mx, my), "%s %dx%d: a click on the left arrow missed", p->name, cw, ch);
                            FcDrawnAt((float)VbxRightArrowX(&b, VBX_X0) + 4.0f, (float)y + 4.0f + 12.0f, s, cw, ch, &mx, &my);
                            CHECK(FcHitClient(rr, s, cw, ch, mx, my), "%s %dx%d: a click on the right arrow missed", p->name, cw, ch);
                            g_clicks += 2;
                        }
            }
    }
}

// a synthetic page: rows x values with widths from a small generator
static unsigned g_seed = 12345u;
static unsigned Rnd(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }
static void Synthetic(void)
{
    static char names[400][24];
    int i;
    for (i = 0; i < 400; ++i) {
        Page p;
        int r, v;
        const float top = i < 100 ? 100.0f : i < 200 ? 140.0f : i < 300 ? 190.0f : 260.0f;  // stock-sized .. far too wide
        memset(&p, 0, sizeof p);
        sprintf(names[i], "synthetic %d", i);
        p.name = names[i];
        p.rows = 1 + (int)(Rnd() % 11);
        for (r = 0; r < p.rows; ++r) {
            p.nv[r] = 1 + (int)(Rnd() % 14);
            p.label[r] = 30.0f + (float)(Rnd() % 1900) / 10.0f;  // up to ~220 px (18 characters)
            for (v = 0; v < p.nv[r]; ++v) p.w[r][v] = 8.0f + (float)(Rnd() % 1000) / 1000.0f * (top - 8.0f);
        }
        CheckPage(&p);
    }
}

// the plan's pages (ol_words.h): value words per (page, row); labels = the plan's row names
typedef struct { const char *page, *row, *v; } PlanV;
typedef struct { const char *page, *row; char kind; } PlanR;
#define XV(pg, rw, rd, vv, vd) { pg, rw, vv },
static const PlanV kPlanV[] = { OL_PLAN_VLINES(XV) { NULL, NULL, NULL } };
#undef XV
#define XR(pg, rw, kd, op) { pg, rw, kd },
static const PlanR kPlanR[] = { OL_PLAN_ROWS(XR) { NULL, NULL, 0 } };
#undef XR
static void Plan(float perChar, const char *model)
{
    static char names[64][64];
    int i, j, np = 0;
    const char *seen[64];
    for (i = 0; kPlanR[i].page; ++i) {
        int k, dup = 0;
        for (k = 0; k < np; ++k)
            if (!strcmp(seen[k], kPlanR[i].page)) dup = 1;
        if (!dup && np < 64) seen[np++] = kPlanR[i].page;
    }
    for (j = 0; j < np; ++j) {
        Page p;
        memset(&p, 0, sizeof p);
        sprintf(names[j], "%s (%s)", seen[j], model);
        p.name = names[j];
        for (i = 0; kPlanR[i].page && p.rows < MAXR; ++i) {
            int k;
            if (strcmp(kPlanR[i].page, seen[j])) continue;
            p.label[p.rows] = perChar * (float)strlen(kPlanR[i].row);
            for (k = 0; kPlanV[k].page; ++k)
                if (!strcmp(kPlanV[k].page, seen[j]) && !strcmp(kPlanV[k].row, kPlanR[i].row) && p.nv[p.rows] < MAXV)
                    p.w[p.rows][p.nv[p.rows]++] = perChar * (float)strlen(kPlanV[k].v);
            ++p.rows;
        }
        CheckPage(&p);
    }
}

static void Fixed(void)
{
    VbxBox b;
    // the stock box
    VbxStock(&b);
    CHECK(b.l == -12 && b.r == 100 && !b.move && !b.shift && b.fits, "the stock box: 344 / 464");
    // the v3.57 log's Sound page (Headphones 117.6 px): the box grows to the right only
    VbxMake(&b, 117.6f, 152.0f);
    CHECK(b.l == -12 && b.r == 118 && !b.move && b.shift == 18, "Sound: right arrow at 482, left at 344 (%d / %d)", VbxLeftArrowX(&b, 364), VbxRightArrowX(&b, 364));
    // Interface (EB Garamond 138 px): past x 500, so 2 px to the left too
    VbxMake(&b, 138.0f, 138.4f);
    CHECK(b.r == VBX_R_MAX && b.l == -14 && b.move == 2 && b.shift == 38 && b.fits, "Interface: %d / %d", VbxLeftArrowX(&b, 364), VbxRightArrowX(&b, 364));
    // a value far wider than the room the labels leave: the left arrow stops at the labels; flagged
    VbxMake(&b, 300.0f, 200.0f);
    CHECK(!b.fits && VbxLeftArrowX(&b, 364) >= 40 + 200 + VBX_LABEL_GAP && b.r == VBX_R_MAX, "too wide: stops at the labels (%d)", VbxLeftArrowX(&b, 364));
    // labels past the stock left arrow (not ours to fix): the stock box, never narrower
    VbxMake(&b, 50.0f, 320.0f);
    CHECK(b.l == VBX_L0 && b.r == VBX_R0, "labels past the stock arrow: the stock box");
    // centring: odd / even room, a value exactly as wide as the room
    VbxStock(&b);
    CHECK(VbxTextX(&b, 364, 100.0f) == 358 && VbxTextX(&b, 364, 0.0f) == 408 && VbxTextX(&b, 364, 33.0f) == 392,
          "centring in 352 .. 464 (%d %d %d)", VbxTextX(&b, 364, 100.0f), VbxTextX(&b, 364, 0.0f), VbxTextX(&b, 364, 33.0f));
    CHECK(VbxSame(&b, &b), "VbxSame");
}

// v3.58 MN358: the pause Extra Menu > Camera page (pm_cam_draw): one ">" x for every value of every row, values centred in
// the room, the room never past x 500; the old rule (">" 6 px after the current value) moved
static void PauseCam(void)
{
    const int vx = 0xFA + 0x12;  // pm_eo_x + 0x12
    int pg, moved = 0;
    for (pg = 0; pg < 300; ++pg) {
        float w[10][42], widest = 0.0f;
        int r, i, n[10], room, gt = -1, olda = -1, oldMoves = 0;
        for (r = 0; r < 10; ++r) {
            n[r] = 1 + (int)(Rnd() % 42);
            for (i = 0; i < n[r]; ++i) {
                w[r][i] = 10.0f + (float)(Rnd() % 1000) / 1000.0f * (pg < 250 ? 120.0f : 400.0f);
                if (w[r][i] > widest) widest = w[r][i];
            }
        }
        room = VbxPmRoom(vx, widest);
        CHECK(VbxPmGtX(vx, room) <= VBX_PM_RIGHT, "pause page %d: > at %d (past 500)", pg, VbxPmGtX(vx, room));
        for (r = 0; r < 10; ++r)
            for (i = 0; i < n[r]; ++i) {
                const int tx = VbxPmTextX(vx, room, w[r][i]), x = VbxPmGtX(vx, room);
                const int old = vx + (int)(w[r][i] + 0.5f) + 6;
                if (gt < 0) gt = x;
                if (olda < 0) olda = old;
                if (old != olda) oldMoves = 1;
                CHECK(x == gt, "pause page %d row %d value %d: > moved (%d, was %d)", pg, r, i, x, gt);
                if (widest + VBX_PM_GAP + vx <= VBX_PM_RIGHT) {
                    const float gl = (float)(tx - vx), gr = (float)(vx + room) - ((float)tx + w[r][i]);
                    CHECK(gl >= -0.01f && gr >= -1.01f && fabsf(gl - gr) <= 1.01f && (float)tx + w[r][i] + VBX_PM_GAP <= (float)x + 1.01f,
                          "pause page %d row %d value %d (%.1f px): centred in the room (%.1f / %.1f)", pg, r, i, w[r][i], gl, gr);
                }
            }
        moved += oldMoves;
    }
    CHECK(moved > 0, "the old rule moved > (the case the fix is for)");
    printf("pause Camera page: 300 pages, one > x each (the old rule moved it on %d)\n", moved);
}

int main(void)
{
    Fixed();
    PauseCam();
    Synthetic();
    Plan(10.4f, "10.4 px a character");
    Plan(13.0f, "13 px a character");
    printf("value box: %d pages (%d with a grown box), %d arrow clicks at 4:3 .. 32:9 (0 missed if OK)\n", g_pages, g_moved, g_clicks);
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "OK", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
