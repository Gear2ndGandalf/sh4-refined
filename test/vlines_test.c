// v3.56 MN356: native test of src\vlines_sim.h and of the plan's value lines in src\ol_words.h (test\build_vlines_test.cmd).
// USER_FEEDBACK_v355 issue 3: every value of every row has its own "Value: what it does." line. Checks:
//   - the byte-mode compare, the ring of text ids (a shown value keeps its id; the 17th pair takes the oldest slot), the
//     decision (only when the row's code answered the row's line, only for a value the plan has a line for), the order
//     rule for stock words (Blood Colour's first three);
//   - OL_PLAN_VLINES: every line starts with its value's word and ": ", fits 52 characters, a row has at most VL_VMAX lines,
//     the rows fit VL_ROWS;
//   - coverage (the house style): every named value of every value row in OL_WORDS_LIST has a line (_VD_), except the
//     numbers and ranges (a word starting with a digit, "..."), the rows listed below with their reason, and the rows
//     another worker of this round owns. Prints what it exempts.
#include <stdio.h>
#include <string.h>
#include "../src/ol_words.h"
#include "../src/vlines_sim.h"

#define VL_ROWS 100  // as src\vlines.inc
#define VL_VMAX 16

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void Enc(unsigned char *out, size_t n, const char *s)  // sh4windowmod.c Encode
{
    size_t k = 0;
    out[k++] = 0x00;
    out[k++] = 0x80;
    for (; *s && k + 2 < n; ++s) out[k++] = *s == ' ' ? 0 : (unsigned char)(0x100 - (unsigned char)*s);
    out[k++] = 0xFF;
    out[k] = 0xFF;
}

static void TestSame(void)
{
    unsigned char a[64], b[64], c[64], d[80];
    Enc(a, sizeof a, "Kills sometimes leave an item.");
    Enc(b, sizeof b, "Kills sometimes leave an item.");
    Enc(c, sizeof c, "Kills sometimes leave an item");
    Enc(d, sizeof d, "Kills sometimes leave an item. Next room.");
    CHECK(VlSame(a, b, 64), "the same text");
    CHECK(!VlSame(a, c, 64) && !VlSame(c, a, 64), "a shorter text is not the same");
    CHECK(!VlSame(a, d, 80) && !VlSame(d, a, 80), "the tagged line is another text");
    CHECK(!VlSame(a, NULL, 64) && !VlSame(NULL, a, 64), "NULL");
    memset(c, 0x41, sizeof c);
    CHECK(!VlSame(c, c, 64), "no end within max: not ours");
    Enc(a, sizeof a, "");
    Enc(b, sizeof b, "");
    CHECK(VlSame(a, b, 64), "two empty texts");
    Enc(b, sizeof b, "Off");
    CHECK(!VlSame(a, b, 64) && !VlSame(b, a, 64), "empty vs Off");
}

static void TestRing(void)
{
    VlRing r;
    int s, s2, fresh, k;
    VlRingInit(&r);
    s = VlRingSlot(&r, 3, 0, &fresh);
    CHECK(fresh && s == 0, "the first pair takes slot 0 (%d %d)", s, fresh);
    s2 = VlRingSlot(&r, 3, 0, &fresh);
    CHECK(!fresh && s2 == s, "a value shown again keeps its slot (its id) frame after frame");
    for (k = 1; k < VL_RING; ++k) {
        s = VlRingSlot(&r, 10 + k, 1, &fresh);
        CHECK(fresh && s == k, "pair %d takes slot %d (%d)", k, k, s);
    }
    s = VlRingSlot(&r, 3, 0, &fresh);
    CHECK(!fresh && s == 0, "the 16 pairs all stay");
    s = VlRingSlot(&r, 99, 2, &fresh);
    CHECK(fresh && s == 0, "the 17th pair takes the oldest slot (%d)", s);
    s = VlRingSlot(&r, 3, 0, &fresh);
    CHECK(fresh && s == 1, "the evicted pair comes back in the next slot (%d)", s);
}

static void TestPick(void)
{
    int m[10], v;
    CHECK(VlPick(1, 0, 4, 0) == 0, "Supply Drops Off: the row's line came back, the plan has Off's line: Off's line");
    CHECK(VlPick(0, 0, 4, 0) == -1, "the row's code answered something else (its own value line, a state line): it stays");
    CHECK(VlPick(1, 2, 4, -1) == -1, "no plan line for the value (a number): the row's line stays");
    CHECK(VlPick(1, -1, 4, 0) == -1 && VlPick(1, 4, 4, 0) == -1, "a getter out of range: the row's own answer");
    for (v = 0; v < 10; ++v) m[v] = v < 3 ? VL_M_STOCK : v;
    CHECK(VlOrderOk(m, 10, 10), "Blood Colour: 3 stock words, 7 of ours in the plan's places: by place");
    m[5] = 6;
    CHECK(!VlOrderOk(m, 10, 10), "a word of ours out of its place: no order rule");
    m[5] = VL_M_NONE;
    CHECK(!VlOrderOk(m, 10, 10), "a word of ours the plan does not list: no order rule");
    for (v = 0; v < 2; ++v) m[v] = VL_M_STOCK;
    CHECK(!VlOrderOk(m, 2, 2), "only stock words (the Film Grain switch): nothing confirms the order");
    for (v = 0; v < 10; ++v) m[v] = v;
    CHECK(!VlOrderOk(m, 3, 10), "the row has 3 values, the plan 10: no order rule");
}

typedef struct { const char *pp, *pn, *d, *l, *vd; } Plan;
static const Plan kPlan[] = {
#define P(pp, pn, d, l, vd) { pp, pn, d, l, vd },
    OL_PLAN_VLINES(P)
#undef P
};
#define PLAN_N ((int)(sizeof kPlan / sizeof kPlan[0]))

static void TestPlan(void)
{
    int i, rows = 0, run = 0, most = 0;
    for (i = 0; i < PLAN_N; ++i) {
        const size_t ll = strlen(kPlan[i].l);
        const int first = i == 0 || strcmp(kPlan[i].pp, kPlan[i - 1].pp) || strcmp(kPlan[i].pn, kPlan[i - 1].pn);
        int j;
        CHECK(strlen(kPlan[i].vd) <= 52, "%s / %s / %s: %d > 52", kPlan[i].pp, kPlan[i].pn, kPlan[i].l, (int)strlen(kPlan[i].vd));
        // house style: "Value: ..." (a value shown shortened may start with its whole name: Cormorant Garamond, Crimson Text)
        CHECK((!strncmp(kPlan[i].vd, kPlan[i].l, ll) && kPlan[i].vd[ll] == ':') ||
              (!strncmp(kPlan[i].vd, kPlan[i].l, ll) && kPlan[i].vd[ll] == ' ' && strchr(kPlan[i].vd, ':')),
              "%s / %s: \"%s\" does not start with \"%s:\"", kPlan[i].pp, kPlan[i].pn, kPlan[i].vd, kPlan[i].l);
        CHECK(strcmp(kPlan[i].vd, kPlan[i].d) != 0, "%s / %s / %s: the value line is the row's line", kPlan[i].pp, kPlan[i].pn, kPlan[i].l);
        if (first) {
            ++rows;
            run = 0;
            for (j = 0; j < i; ++j)
                CHECK(strcmp(kPlan[j].pp, kPlan[i].pp) || strcmp(kPlan[j].pn, kPlan[i].pn), "%s / %s: its lines are not together",
                      kPlan[i].pp, kPlan[i].pn);
        }
        if (++run > most) most = run;
    }
    CHECK(rows <= VL_ROWS, "%d rows with value lines > VL_ROWS %d", rows, VL_ROWS);
    CHECK(most <= VL_VMAX, "a row with %d value lines > VL_VMAX %d", most, VL_VMAX);
    CHECK(PLAN_N >= 270, "the plan's value lines: %d", PLAN_N);
    printf("plan: %d value lines in %d rows (at most %d a row)\n", PLAN_N, rows, most);
}

// coverage: every OLW_<ROW>_V_<VALUE> (a value row's word) has OLW_<ROW>_VD_<VALUE>, unless exempt
typedef struct { char kind; const char *text, *name; } Word;
static const Word kWords[] = {
#define W(k, t, n) { k, t, n },
    OL_WORDS_LIST(W)
#undef W
};
#define WORDS_N ((int)(sizeof kWords / sizeof kWords[0]))
// rows whose values are a range or a scale (their row line names them), one fixed value, or another worker's this round
static const struct { const char *row, *why; } kExempt[] = {
    { "OLW_CAMERA_SIDE_", "positions Left 150 .. Right 150 (a range of 13)" },
    { "OLW_INTERFACE_LANGUAGE_", "one value (English)" },
    // v3.56 MERGE: the Randomizer page (RI356 / RE356) and Enemies Return (ER356) are built: no longer exempt
};
#define EXEMPT_N ((int)(sizeof kExempt / sizeof kExempt[0]))

static int Has(const char *name)
{
    int i;
    for (i = 0; i < WORDS_N; ++i)
        if (!strcmp(kWords[i].name, name)) return 1;
    return 0;
}

static void TestCoverage(void)
{
    int i, e, values = 0, lines = 0, numbers = 0, exempt[EXEMPT_N] = { 0 };
    for (i = 0; i < WORDS_N; ++i) {
        const char *n = kWords[i].name, *v = strstr(n, "_V_");
        char want[160];
        int ex = -1;
        if (kWords[i].kind != 'V' || !v || !strncmp(n, "OLW_CAM_V_", 10)) continue;  // (OLW_CAM_V_*: the EXTRA state words)
        ++values;
        for (e = 0; e < EXEMPT_N; ++e)
            if (!strncmp(n, kExempt[e].row, strlen(kExempt[e].row))) ex = e;
        if (ex >= 0) {
            ++exempt[ex];
            continue;
        }
        if ((kWords[i].text[0] >= '0' && kWords[i].text[0] <= '9') || !strcmp(kWords[i].text, "...")) {
            sprintf_s(want, sizeof want, "%.*s_VD_%s", (int)(v - n), n, v + 3);
            if (!Has(want)) ++numbers;  // a number keeps the row's line (the row names its scale)
            else ++lines;
            continue;
        }
        sprintf_s(want, sizeof want, "%.*s_VD_%s", (int)(v - n), n, v + 3);
        CHECK(Has(want), "%s (\"%s\") has no value line %s: it would show the row's line", n, kWords[i].text, want);
        if (Has(want)) ++lines;
    }
    printf("coverage: %d value words: %d with their own line, %d numbers keep the row's line", values, lines, numbers);
    for (e = 0; e < EXEMPT_N; ++e) printf(", %d %s (%s)", exempt[e], kExempt[e].row, kExempt[e].why);
    printf("\n");
    CHECK(Has("OLW_ITEMS_SUPPLY_DROPS_VD_OFF") && !strcmp(OLW_ITEMS_SUPPLY_DROPS_VD_OFF, "Off: kills leave nothing, as the game."),
          "issue 3: Supply Drops Off has its own line");
    CHECK(!strcmp(OLW_PAUSE_EILEEN_L, "Eileen"), "QA item 9: the pause row is \"Eileen\" (%s)", OLW_PAUSE_EILEEN_L);
    CHECK(!strcmp(OLW_RANDO_EILEEN_OUTFIT_L, "Eileen Outfit"), "the Randomizer's own Eileen Outfit row stays");
}

int main(void)
{
    TestSame();
    TestRing();
    TestPick();
    TestPlan();
    TestCoverage();
    printf("vlines_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
