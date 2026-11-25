// v3.35 FX335: native test of src\budget_sim.h (test\build_budget_test.cmd): the effect budget's governor at 30 / 60 / 120
// / 180 frames a second (the same moments in seconds), load spikes and hitches that must not trip it, recovery and its
// back-off, the levels' order and the skipped levels, the copies After Shock keeps, Death Presence's puff share, the
// fades, and the ledger's "top costs" text (logcheck.py reads it inside parentheses: none in it).
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/budget_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const unsigned kAll = (1u << BGL_N) - 1u;
// Echo x1, After Shock Long, Death Presence Strong (the user's screenshot): no 2nd / 3rd shadow to thin
static const unsigned kUser = 1u | 1u << BGL_AI5 | 1u << BGL_ECHOALL | 1u << BGL_AI3DP | 1u << BGL_DP50 | 1u << BGL_ECHO1;

static void TestTarget(void)
{
    CHECK(fabsf(BgTargetMs(6.0f, 1000.0f / 60.0f) - 6.0f) < 1e-3f, "60 fps: 6 ms (%f)", BgTargetMs(6.0f, 1000.0f / 60.0f));
    CHECK(fabsf(BgTargetMs(6.0f, 1000.0f / 30.0f) - 12.0f) < 1e-3f, "30 fps: 12 ms");
    CHECK(fabsf(BgTargetMs(6.0f, 1000.0f / 120.0f) - 3.0f) < 1e-3f, "120 fps: 3 ms");
    CHECK(fabsf(BgTargetMs(6.0f, 1000.0f / 180.0f) - 2.0f) < 1e-3f, "180 fps: 2 ms");
    CHECK(BgTargetMs(12.0f, 1000.0f / 30.0f) == BG_T_MAX, "clamped high");
    CHECK(BgTargetMs(2.0f, 1000.0f / 240.0f) == BG_T_MIN, "clamped low");
    CHECK(fabsf(BgTargetMs(0.0f, 0.0f) - BG_T60_MS) < 1e-3f, "bad input: the 60 fps default");
}

// one run: `secs` seconds at `fps`; cost(t, level) = the effects' ms; late(t, level) = the frame's length. Returns the
// level at the end; first: the first step down's time (s), maxL the deepest level, changes the steps.
typedef float (*CostFn)(float t, int level, float frameT);
typedef float (*FrameFn)(float t, int level, float frameT, float cost);
static int Run(BgGov *g, int fps, float secs, unsigned useful, CostFn cost, FrameFn frame, float *first, int *maxL, int *changes,
               float *backAt)
{
    const float ft = 1000.0f / (float)fps;
    float t = 0.0f;  // real time: a late frame lasts longer
    if (first) *first = -1.0f;
    if (maxL) *maxL = g->level;
    if (changes) *changes = 0;
    if (backAt) *backAt = -1.0f;
    for (; t < secs; ) {
        const float c = cost(t, g->level, ft), ms = frame(t, g->level, ft, c);
        const int s = BgGovStep(g, ms / 1000.0f, ms, c, ft, BG_T60_MS, useful, 0);
        if (s && changes) ++*changes;
        if (s > 0 && first && *first < 0.0f) *first = t;
        if (maxL && g->level > *maxL) *maxL = g->level;
        if (s < 0 && g->level == 0 && backAt && *backAt < 0.0f) *backAt = t;
        CHECK(g->level >= 0 && g->level <= BG_MAX, "level in range (%d)", g->level);
        t += ms / 1000.0f;
    }
    return g->level;
}
// a light scene: the effects 3 ms at 60 fps (scaled), every frame on time
static float CostLight(float t, int L, float ft) { (void)t, (void)L; return 3.0f * ft / 16.667f; }
static float FrameOk(float t, int L, float ft, float c) { (void)t, (void)L, (void)c; return ft; }
// a fight from 2 s to 20 s: the effects 14 ms at level 0, each level saves 15 % (at most 70 %); a frame is late (twice
// the target) while the game's 8 ms + the effects exceed the frame
static float CostFight(float t, int L, float ft)
{
    const float k = ft / 16.667f, base = t >= 2.0f && t < 20.0f ? 14.0f : 3.0f;
    float save = 0.15f * (float)L;
    if (save > 0.7f) save = 0.7f;
    return base * (1.0f - save) * k;
}
static float FrameFight(float t, int L, float ft, float c)
{
    (void)t, (void)L;
    return 8.0f * ft / 16.667f + c > ft ? 2.0f * ft : ft;
}

static void TestLight(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kAll, CostLight, FrameOk, NULL, &maxL, &ch, NULL);
    CHECK(maxL == 0 && ch == 0, "fps %d: a light scene never thins (max %d, %d changes)", fps, maxL, ch);
}

static void TestFight(int fps, float *firstOut, float *backOut, int *maxOut)
{
    BgGov g;
    float first, back;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kAll, CostFight, FrameFight, &first, &maxL, &ch, &back);
    CHECK(first >= 2.0f + BG_DOWN_S - 0.05f && first <= 2.0f + 1.25f, "fps %d: the first step down at %.2f s (the fight from 2 s; want ~0.8 .. 1.25 s into it)", fps, first);
    CHECK(maxL >= 3 && maxL <= 6, "fps %d: thinned enough to be on time again (max level %d)", fps, maxL);
    CHECK(g.level == 0, "fps %d: everything back at the end (level %d)", fps, g.level);
    CHECK(back > 20.0f && back < 20.0f + (BG_UP_S + 0.5f) * (float)maxL + 4.0f, "fps %d: back to full at %.1f s (the fight ends at 20 s, %d levels)", fps, back, maxL);
    CHECK(ch <= 2 * maxL + 6, "fps %d: no flicker (%d changes for max level %d)", fps, ch, maxL);
    *firstOut = first, *backOut = back, *maxOut = maxL;
}

// the user's settings (Echo x1): the levels without anything to thin are skipped
static void TestUseful(void)
{
    BgGov g;
    int seen[BGL_N], f, i;
    memset(seen, 0, sizeof seen);
    BgGovReset(&g);
    for (f = 0; f < 60 * 30; ++f) {  // 30 s of a heavy fight at 60 fps (never on time)
        BgGovStep(&g, 1.0f / 60.0f, 33.3f, 20.0f, 16.667f, BG_T60_MS, kUser, 0);
        seen[g.level] = 1;
    }
    for (i = 1; i < BGL_N; ++i)
        CHECK(seen[i] == ((kUser >> i & 1u) != 0), "level %d %s with Echo x1 (seen %d)", i, (kUser >> i & 1u) ? "taken" : "skipped", seen[i]);
    CHECK(g.level == BG_MAX, "the deepest useful level reached (%d)", g.level);
    BgGovReset(&g);
    for (f = 0; f < 600; ++f) BgGovStep(&g, 1.0f / 60.0f, 33.3f, 20.0f, 16.667f, BG_T60_MS, 1u, 0);
    CHECK(g.level == 0, "nothing on: no level to take (%d)", g.level);
    CHECK(BgNextUp(kUser, 0) == BGL_AI5 && BgNextDown(kUser, BGL_AI5) == 0 && BgNextDown(kUser, BGL_AI3DP) == BGL_ECHOALL, "next levels");
}

// load spikes: a 276 ms frame, a 150 ms one and a few 60 ms ones mid-play (the 1.5 s after a load the caller skips anyway:
// skip = 1), and a short burst of late frames: no step
static void TestSpikes(int fps)
{
    const float ft = 1000.0f / (float)fps;
    BgGov g;
    float t = 0.0f;
    int steps = 0, s5 = 0, s8 = 0;
    BgGovReset(&g);
    while (t < 20.0f) {  // (real time)
        float ms = ft, c = 4.0f * ft / 16.667f;
        int skip = 0;
        if (t >= 5.0f && !s5) s5 = 1, ms = 276.0f, c = 259.0f;            // the user's freeze: a hitch, not counted
        if (t >= 8.0f && !s8) s8 = 1, ms = 150.0f, c = 40.0f;
        if (t >= 10.0f && t < 11.5f) skip = 1, ms = 60.0f, c = 30.0f;     // a load's first 1.5 s: skipped by the caller
        if (t >= 14.0f && t < 14.25f) ms = 2.5f * ft;                    // a quarter second of late frames (a door, a disk read)
        steps += BgGovStep(&g, ms / 1000.0f, ms, c, ft, BG_T60_MS, kAll, skip) != 0;
        t += ms / 1000.0f;
    }
    CHECK(steps == 0 && g.level == 0, "fps %d: spikes, a load and a 0.25 s burst do not trip it (%d steps, level %d)", fps, steps, g.level);
}

// a scene right at the edge: level 0 runs late, level 1 is on time with the cost low; each step up is taken back: the
// back-off must grow (fewer and fewer tries), not flicker every few seconds
static float CostEdge(float t, int L, float ft) { (void)t; return (L == 0 ? 10.0f : 5.0f) * ft / 16.667f; }
static float FrameEdge(float t, int L, float ft, float c) { (void)t, (void)c; return L == 0 ? 1.6f * ft : ft; }
static void TestRebound(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 180.0f, kAll, CostEdge, FrameEdge, NULL, &maxL, &ch, NULL);
    CHECK(ch <= 2 * 12, "fps %d: at most ~12 tries in 3 minutes at the edge (%d changes)", fps, ch);
    CHECK(g.backoff >= BG_UP_MAX - 1e-3f, "fps %d: the back-off grew to its cap (%.1f s)", fps, g.backoff);
    CHECK(maxL == 1, "fps %d: one level is enough (max %d)", fps, maxL);
}

// frames late while the effects cost little (the game's own heavy room, the driver): thinning would not help, so it does not
static float CostTiny(float t, int L, float ft) { (void)t, (void)L; return 1.2f * ft / 16.667f; }
static float FrameHalfLate(float t, int L, float ft, float c) { (void)L, (void)c; return ((int)(t * 30.0f) & 1) ? 1.8f * ft : ft; }
static void TestNotOurs(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kAll, CostTiny, FrameHalfLate, NULL, &maxL, &ch, NULL);
    CHECK(maxL == 0, "fps %d: late frames that are not the effects' doing thin nothing (max %d)", fps, maxL);
}
// the effects alone over twice their target with every frame on time (a fast machine at 30 fps, say): thinned anyway
static float CostHuge(float t, int L, float ft) { (void)t; return (L ? 10.0f : 13.0f) * ft / 16.667f; }
static void TestHard(void)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, 60, 10.0f, kAll, CostHuge, FrameOk, NULL, &maxL, &ch, NULL);
    CHECK(maxL >= 1, "the effects at 13 ms of a 16.7 ms frame: thinned even with every frame on time (max %d)", maxL);
}

static void TestKeep(void)
{
    static const int kN[] = { 3, 5, 8 };
    int a, m, k;
    for (a = 0; a < 3; ++a)
        for (m = 1; m <= kN[a]; ++m) {
            int cnt = 0;
            for (k = 1; k <= kN[a]; ++k) cnt += BgAiKeep(k, kN[a], m);
            CHECK(cnt == m, "n %d m %d: %d kept", kN[a], m, cnt);
            CHECK(BgAiKeep(1, kN[a], m), "n %d m %d: the nearest kept", kN[a], m);
            if (m >= 2) CHECK(BgAiKeep(kN[a], kN[a], m), "n %d m %d: the tail kept (the trail keeps its length)", kN[a], m);
        }
    CHECK(BgAiKeep(1, 8, 5) && !BgAiKeep(2, 8, 5) && BgAiKeep(3, 8, 5) && !BgAiKeep(4, 8, 5) && BgAiKeep(5, 8, 5) && BgAiKeep(6, 8, 5) &&
          !BgAiKeep(7, 8, 5) && BgAiKeep(8, 8, 5), "Long 8 -> 5: copies 1 3 5 6 8");
    CHECK(BgAiCopies(0, 8) == 8 && BgAiCopies(BGL_AI5, 8) == 5 && BgAiCopies(BGL_AI5, 5) == 4 && BgAiCopies(BGL_AI5, 3) == 3, "level 5 copies");
    CHECK(BgAiCopies(BGL_AI3DP, 8) == 3 && BgAiCopies(BGL_AI3DP, 5) == 3 && BgAiCopies(BG_MAX, 3) == 3, "level 7 copies");
    CHECK(BgEchoLevel(0) == 0 && BgEchoLevel(BGL_ECHO3) == 0 && BgEchoLevel(BGL_ECHOWEP) == 1 && BgEchoLevel(BGL_ECHOLITE) == 2 &&
          BgEchoLevel(BGL_ECHO2) == 2 && BgEchoLevel(BGL_ECHOALL) == 3 && BgEchoLevel(BG_MAX) == 3, "Echo's own levels in order");
    CHECK(BgEchoBodies(0) == 3 && BgEchoBodies(BGL_ECHO3) == 2 && BgEchoBodies(BGL_ECHO2) == 1 && BgEchoBodies(BGL_DP50) == 1 &&
          BgEchoBodies(BGL_ECHO1) == 0, "Echo's bodies by level");
    CHECK(BgDpKeep(0) == 1.0f && BgDpKeep(BGL_AI3DP) == 0.7f && BgDpKeep(BGL_DP50) == 0.5f && BgDpKeep(BG_MAX) == 0.5f, "Death Presence's share");
}

static void TestPuffs(void)
{
    const int n = 160;
    float k;
    for (k = 0.5f; k <= 1.001f; k += 0.05f) {
        float sum = 0.0f, prev = 2.0f;
        int i, mono = 1;
        for (i = 0; i < n; ++i) {
            const float p = BgDpPuffK(i, n, k);
            sum += p;
            if (p > prev + 1e-6f) mono = 0;
            prev = p;
        }
        CHECK(mono, "k %.2f: the share falls with the index (the last ones fade first)", k);
        CHECK(fabsf(sum / (float)n - k) < 0.03f, "k %.2f: about that share kept (%.3f)", k, sum / (float)n);
    }
    CHECK(BgDpPuffK(n - 1, n, 1.0f) == 1.0f, "full share: every puff");
}

static void TestRamp(void)
{
    static const int kFps[4] = { 30, 60, 120, 180 };
    int r;
    for (r = 0; r < 4; ++r) {
        float v = 1.0f, t = 0.0f;
        int f;
        for (f = 0; f < kFps[r] * 2 && v > 0.0f; ++f) v = BgRamp(v, 0.0f, 1.0f / (float)kFps[r], 2.5f), t += 1.0f / (float)kFps[r];
        CHECK(fabsf(t - 0.4f) <= 1.0f / (float)kFps[r] + 1e-4f, "fps %d: a 0.4 s fade (%.3f s)", kFps[r], t);
    }
    CHECK(BgRamp(0.3f, 1.0f, 0.0f, 2.5f) == 0.3f && BgRamp(0.3f, 1.0f, 0.6f, 2.5f) == 0.3f, "no time / a hitch: no step");
}

static void TestText(void)
{
    float ms[BGC_N];
    char s[128];
    int n;
    memset(ms, 0, sizeof ms);
    ms[BGC_ECHO] = 21.34f, ms[BGC_AI] = 8.08f, ms[BGC_DP] = 3.21f, ms[BGC_FX] = 0.9f, ms[BGC_T_ENM] = 0.2f;
    n = BgTopText(ms, 3, 0.3f, s, sizeof s);
    CHECK(n == 3 && !strcmp(s, "echo 21.3 ms, after shock 8.1, death presence 3.2"), "top three (%d: %s)", n, s);
    CHECK(!strchr(s, '(') && !strchr(s, ')'), "no parentheses (logcheck.py's room-line field)");
    n = BgTopText(ms, 5, 0.3f, s, sizeof s);
    CHECK(n == 4 && strstr(s, "screen effects 0.9") != NULL && !strstr(s, "enemies"), "under the floor: not named (%s)", s);
    memset(ms, 0, sizeof ms);
    CHECK(BgTopText(ms, 3, 0.3f, s, sizeof s) == 0 && s[0] == 0, "nothing: empty");
    ms[BGC_DFX] = 2.0f, ms[BGC_ECHO] = 1.0f, ms[BGC_T_AI] = 5.0f;
    CHECK(fabsf(BgEffectsMs(ms) - 3.0f) < 1e-4f && fabsf(BgTotalMs(ms) - 8.0f) < 1e-4f, "the effects' sum excludes the ticks");
    {
        int i;
        for (i = 0; i < BGC_N; ++i) CHECK(kBgName[i] && kBgName[i][0] && !strchr(kBgName[i], '(') && !strchr(kBgName[i], ';'), "name %d", i);
        for (i = 0; i < BGL_N; ++i)
            CHECK(kBgLevelText[i] && kBgLevelText[i][0] && !strchr(kBgLevelText[i], '(') && !strchr(kBgLevelText[i], ')'),
                  "level text %d (no parentheses: logcheck.py reads it inside the budget room line's)", i);
    }
}

int main(void)
{
    static const int kFps[4] = { 30, 60, 120, 180 };
    float first[4], back[4];
    int maxL[4], r;
    TestTarget();
    for (r = 0; r < 4; ++r) TestLight(kFps[r]), TestSpikes(kFps[r]), TestRebound(kFps[r]), TestNotOurs(kFps[r]);
    TestHard();
    for (r = 0; r < 4; ++r) TestFight(kFps[r], &first[r], &back[r], &maxL[r]);
    for (r = 1; r < 4; ++r) {  // the same moments in seconds at every rate
        CHECK(fabsf(first[r] - first[1]) <= 0.1f, "fps %d: the first step at %.2f s (60 fps %.2f)", kFps[r], first[r], first[1]);
        CHECK(maxL[r] == maxL[1], "fps %d: the same deepest level (%d, 60 fps %d)", kFps[r], maxL[r], maxL[1]);
        CHECK(fabsf(back[r] - back[1]) <= 1.0f, "fps %d: back to full at %.1f s (60 fps %.1f)", kFps[r], back[r], back[1]);
    }
    printf("fight: first step %.2f / %.2f / %.2f / %.2f s, deepest %d / %d / %d / %d, back to full %.1f / %.1f / %.1f / %.1f s (30 / 60 / 120 / 180 fps)\n",
           first[0], first[1], first[2], first[3], maxL[0], maxL[1], maxL[2], maxL[3], back[0], back[1], back[2], back[3]);
    TestUseful();
    TestKeep();
    TestPuffs();
    TestRamp();
    TestText();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "budget_sim_test ok", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
