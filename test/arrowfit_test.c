// v3.33 MENU333: native test of src\arrowfit_sim.h (test\build_arrowfit_test.cmd): the right value arrow of the selected
// Options row. Widths are the game's font 0 metrics of the real values (tools\menucheck_values.py on fontdata_kr.bin,
// LabelW's formula), so the cases below are the rows the v3.32 rules left under the arrow plus the ones that must not
// change. The canvas is 512 wide at every Aspect Ratio: the same numbers hold at 4:3, 16:9, 21:9 and 32:9.
#include <stdio.h>
#include "../src/arrowfit_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { const char *row; int n; float w[10]; int old, want; } Row;

int main(void)
{
    static const Row rows[] = {
        { "Echo Doubles (Off / Shadow Copy / Shadow Cast)", 3, { 34.0f, 130.0f, 124.4f }, 0, 36 },
        { "Curse Meter (Off / On / Companion / Both)", 4, { 34.0f, 30.0f, 113.2f, 46.4f }, 0, 20 },
        { "Supply Drops (Off / Normal / Generous)", 3, { 34.0f, 72.0f, 95.2f }, 0, 2 },
        { "Display Mode (old rule 36 kept)", 2, { 103.2f, 102.4f }, 36, 36 },
        { "Screen Effects Filter (Night Vision Mono moves the column)", 10,
          { 50, 47, 52, 53, 88, 122.8f, 186.8f, 82, 126.8f, 85 }, 36, 36 },
        { "Enemies Follow (Monsters + Victims moves the column)", 3, { 34.0f, 92.0f, 199.2f }, 0, 0 },
        { "Enemy Health Bars (Locked Target moves the column)", 3, { 34.0f, 142.8f, 80.8f }, 0, 0 },
        { "Movement Speed (fits)", 4, { 72.0f, 53.0f, 42.0f, 23.0f }, 0, 0 },
        { "Subtitle Edge (Outline+Shadow moves the column)", 4, { 72.0f, 50.0f, 77.0f, 167.2f }, 36, 36 },
    };
    int i, k;
    CHECK(AfNeed(94.0f) == 0, "94 px fits (need %d)", AfNeed(94.0f));
    CHECK(AfNeed(94.5f) == 1, "94.5 px needs 1 (got %d)", AfNeed(94.5f));
    CHECK(AfNeed(130.0f) == AF_MAXSHIFT, "130 px needs the full 36 (got %d)", AfNeed(130.0f));
    CHECK(AF_COLW == 130, "the column room is LongMove's 130 (%d)", AF_COLW);
    CHECK(464 + AF_MAXSHIFT + 8 <= 508, "the shifted arrow ends inside the canvas (by 508)");
    for (i = 0; i < (int)(sizeof rows / sizeof rows[0]); ++i) {
        const Row *r = &rows[i];
        const int s = AfRowShift(r->w, r->n, r->old);
        CHECK(s == r->want, "%s: shift %d, want %d", r->row, s, r->want);
        CHECK(s >= r->old, "%s: never less than the old rule", r->row);
        CHECK(s <= AF_MAXSHIFT || s == r->old, "%s: at most %d unless the old rule said more", r->row, AF_MAXSHIFT);
        for (k = 0; k < r->n; ++k) {
            if (r->w[k] > (float)AF_COLW) {
                // LongValW moves the column LongMove(w) left; the arrow 6 px after the text, inside the canvas
                const int move = (int)r->w[k] - 130 > 64 ? (int)r->w[k] - 130 : 64;
                const float x = 364.0f - (float)move, arrow = 364.0f + r->w[k] + 6.0f - (float)move + 0.0f;
                CHECK(x - 20.0f >= 0.0f && arrow + 8.0f <= 512.0f + 0.5f && arrow >= x + r->w[k] + 2.0f,
                      "%s value %d (%.0f px): moved column fits (x %.0f, arrow %.0f)", r->row, k, r->w[k], x, arrow);
                continue;
            }
            CHECK(!AfOverlaps(r->w[k], s), "%s value %d (%.0f px) clear of the arrow at +%d", r->row, k, r->w[k], s);
        }
    }
    // the arrow does not move while the value changes (one shift per row)
    {
        const float w[3] = { 34.0f, 130.0f, 124.4f };
        CHECK(AfRowShift(w, 1, 0) == 0 && AfRowShift(w, 3, 0) == 36, "the widest value decides, not the current one");
    }
    // v3.32 overlaps (no shift for these rows) are gone
    CHECK(AfOverlaps(124.4f, 0) && AfOverlaps(130.0f, 0) && AfOverlaps(113.2f, 0), "v3.32: Shadow Cast / Copy / Companion overlapped");
    printf("arrowfit_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
