// v3.56 MN356: native test of src\folderclick_sim.h (test\build_folderclick_test.cmd). USER_FEEDBACK_v355 issue 1: one
// click on a folder row's arrow or label opens it every time it is not mid-slide. For every shape the Aspect Ratio row
// offers (4:3, 16:10, 16:9, 21:9, 32:9), window sizes from 640x480 to 5120x1440 (and odd ones), pages of 7 .. 11 rows (the
// layout's pitches 32 / 28 / 26 / 24) and row heights 24 .. the pitch, a click drawn on
//   - the blue triangle (x 364 .. 372, LK_FOLDER_X), its centre and its corners,
//   - the label (from x 40 across the widest label any font draws: the three HD fonts keep the stock font's metrics, so
//     one width bound holds for all four),
//   - the right arrow's place (464 .. 484) and the value column,
// maps (Refined's HitRectI + the game's 0x405690) into that row's open box and the row is the one the stock hover finds;
// a combo row opens on its label only; a click right of x 484 or between pages' rows opens nothing; a click mid-slide is
// refused and the next one opens.
#include <stdio.h>
#include <math.h>
#include "../src/folderclick_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; if (g_fail < 40) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } } while (0)

static int Pitch(int n) { return n <= 8 ? 32 : n == 9 ? 28 : n == 10 ? 26 : n == 11 ? 24 : 22; }  // layout_sim.h OlPitch

// the widest label: 18 characters (PLAN section 3), at most 14 canvas units a character with the letter spacing (font 0;
// LabelW measured 10.4 a character on "Mono"; the widest capitals stay under 14): 252. The HD fonts draw into the stock
// metrics (font.inc: "every width, kerning pair, baseline and cap height kept"), so the bound is the same for every font.
#define LABEL_W_MAX (18 * 14)

static void TestUnits(void)
{
    int box[4], inside[5] = { 0, 1, 0, 1, 0 };
    CHECK(FcBox(FC_C_ROW, 40, 96, 24, box) && box[0] == 32 && box[0] + box[2] == FC_RIGHT && box[1] == 96 && box[3] == 24, "a folder's box");
    CHECK(FcBox(FC_C_LABEL, 40, 96, 24, box) && box[0] + box[2] == FC_COL_X, "a combo's box: its label only");
    CHECK(!FcBox(FC_C_NONE, 40, 96, 24, box) && !FcBox(FC_C_ROW, 40, 96, 0, box), "no box: a value row, an empty rect");
    CHECK(FcHoverBox(40, 96, 24, box) && box[2] == FC_HOVER_RIGHT - 40, "the stock hover box: 40 .. 464");
    CHECK(FcPickByHit(inside, 5) == 3, "the last row holding the point wins, as the stock hover");
    inside[1] = inside[3] = 0;
    CHECK(FcPickByHit(inside, 5) == -1, "no row");
    CHECK(FcDecide(0, 2, 1, 0) == FC_STOCK, "no click: the stock test");
    CHECK(FcDecide(1, -1, 0, 0) == FC_STOCK, "a click off every row: the stock test");
    CHECK(FcDecide(1, 2, 0, 0) == FC_STOCK, "a click on a value row (or a combo's value): the stock test");
    CHECK(FcDecide(1, 2, 1, 0) == FC_OPEN, "a click on a folder row: open");
    CHECK(FcDecide(1, 2, 1, 1) == FC_REFUSE, "mid-slide: refused (logged)");
    CHECK(FcDecide(1, 2, 1, 0) == FC_OPEN, "the next click after the slide: open");
    CHECK(FC_COL_X > FC_ROW_X + LABEL_W_MAX, "the widest label (%d) ends before the value column (%d)", FC_ROW_X + LABEL_W_MAX, FC_COL_X);
    CHECK(FC_COL_X == 512 - 40 - 100 - 8 - 20 && FC_RIGHT == 512 - 40 - 100 - 8 + 100 + 20, "the stock column (0x525C1B .. 0x525CD4)");
}

// the row whose open box / hover box holds the client point, as menuclick.inc FcClickHit decides it
static int Decide(const int *cls, int n, int y0, int pitch, int h, float s, int cw, int ch, int mx, int my, int busy, int *row)
{
    int i, inside[16], open[16], box[4];
    for (i = 0; i < n; ++i) {
        inside[i] = open[i] = 0;
        if (FcBox(cls[i], FC_ROW_X, y0 + pitch * i, h, box)) open[i] = FcHitClient(box, s, cw, ch, mx, my);
        inside[i] = open[i] || (FcHoverBox(FC_ROW_X, y0 + pitch * i, h, box) && FcHitClient(box, s, cw, ch, mx, my));
    }
    *row = FcPickByHit(inside, n);
    return FcDecide(1, *row, *row >= 0 && open[*row], busy);
}

static void TestShapes(void)
{
    static const struct { const char *name; int w, h; } kSize[] = {
        { "640x480", 640, 480 }, { "800x600", 800, 600 }, { "1024x768", 1024, 768 }, { "784x588", 784, 588 },
        { "1280x800", 1280, 800 }, { "1680x1050", 1680, 1050 }, { "1280x720", 1280, 720 }, { "1920x1080", 1920, 1080 },
        { "2560x1440", 2560, 1440 }, { "3840x2160", 3840, 2160 }, { "2560x1080", 2560, 1080 }, { "3440x1440", 3440, 1440 },
        { "3840x1080", 3840, 1080 }, { "5120x1440", 5120, 1440 }, { "800x627", 800, 627 }, { "1366x768", 1366, 768 },
    };
    const int nSize = (int)(sizeof kSize / sizeof kSize[0]);
    int si, n, h, fi, opened = 0, refused = 0;
    for (si = 0; si < nSize; ++si) {
        const int cw = kSize[si].w, ch = kSize[si].h;
        const float s = (3.0f * (float)cw) / (4.0f * (float)ch);  // sh4windowmod.c g_scale2d
        for (n = 7; n <= 11; ++n) {
            const int pitch = Pitch(n);
            for (h = 24; h <= pitch; h += pitch - 24 > 0 ? pitch - 24 : 1) {
                int cls[16];
                for (fi = 0; fi < n; ++fi) {
                    int k, mx, my, r, got;
                    // the folder row fi among value rows (and, on the other pass, every row a folder)
                    for (k = 0; k < 2; ++k) {
                        int j;
                        const float y = (float)(96 + pitch * fi);
                        static const float kX[] = { 364.5f, 368.0f, 371.5f, 40.0f, 41.0f, 120.0f, 200.0f, FC_ROW_X + LABEL_W_MAX - 1.0f, 300.0f,
                                                    350.0f, 420.0f, 466.0f, 482.0f };
                        for (j = 0; j < n; ++j) cls[j] = k ? FC_C_ROW : (j == fi ? FC_C_ROW : FC_C_NONE);
                        for (j = 0; j < (int)(sizeof kX / sizeof kX[0]); ++j) {
                            // a point inside the glyph's height (rows draw from y + 2 on; the triangle is y + 4 .. y + 28)
                            const float yy = y + (float)(h < 20 ? h / 2 : 6 + (j % 3) * 5);
                            FcDrawnAt(kX[j], yy, s, cw, ch, &mx, &my);
                            r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 0, &got);
                            CHECK(r == FC_OPEN && got == fi, "%s, %d rows, h %d: a click drawn at (%.1f, %.1f) on folder row %d -> %d row %d",
                                  kSize[si].name, n, h, kX[j], yy, fi, r, got);
                            if (r == FC_OPEN) ++opened;
                            r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 1, &got);
                            CHECK(r == FC_REFUSE, "%s: mid-slide refused", kSize[si].name);
                            if (r == FC_REFUSE) ++refused;
                        }
                        // right of the right arrow's place: nothing
                        FcDrawnAt(490.0f, y + 10.0f, s, cw, ch, &mx, &my);
                        r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 0, &got);
                        CHECK(r == FC_STOCK, "%s: x 490 opens nothing (%d)", kSize[si].name, r);
                    }
                    // a combo row (StockMoves=0's Subtitles): its label opens, its value column and arrows stay the stock test
                    {
                        int j;
                        for (j = 0; j < n; ++j) cls[j] = j == fi ? FC_C_LABEL : FC_C_NONE;
                        FcDrawnAt(150.0f, (float)(96 + pitch * fi) + 10.0f, s, cw, ch, &mx, &my);
                        r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 0, &got);
                        CHECK(r == FC_OPEN && got == fi, "%s: a combo's label opens (%d %d)", kSize[si].name, r, got);
                        FcDrawnAt(400.0f, (float)(96 + pitch * fi) + 10.0f, s, cw, ch, &mx, &my);
                        r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 0, &got);
                        CHECK(r == FC_STOCK, "%s: a combo's value column keeps the stock meaning (%d)", kSize[si].name, r);
                        FcDrawnAt(470.0f, (float)(96 + pitch * fi) + 10.0f, s, cw, ch, &mx, &my);
                        r = Decide(cls, n, 96, pitch, h, s, cw, ch, mx, my, 0, &got);
                        CHECK(r == FC_STOCK, "%s: a combo's right arrow keeps the stock meaning (%d)", kSize[si].name, r);
                    }
                }
                if (pitch == 24) break;
            }
        }
    }
    printf("shapes: %d sizes (4:3 .. 32:9), 7 .. 11 rows: %d clicks opened their folder row, %d refused mid-slide\n", nSize,
           opened, refused);
}

int main(void)
{
    TestUnits();
    TestShapes();
    printf("folderclick_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
