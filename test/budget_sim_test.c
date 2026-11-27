// v3.35 FX335: native test of src\budget_sim.h (test\build_budget_test.cmd): the effect budget's governor at 30 / 60 / 120
// / 180 frames a second (the same moments in seconds), load spikes and hitches that must not trip it, recovery and its
// back-off, the levels' order and the skipped levels, the copies After Shock keeps, Death Presence's puff share, the
// fades, and the ledger's "top costs" text (logcheck.py reads it inside parentheses: none in it).
// v3.56 PW356: the v3.55 checks run with the classic levels (kClassic: the spark levels are a high rate's); a regression
// check against v3.55's governor (budget_ref355.h) below 115 fps and with the high-rate rules off at any rate; the
// high-rate fallback: blind steps kept when they help and taken back when not (with the growing hold-off), no step
// without a power drawing, jitter that is not a miss, evidence steps, a rate drop mid-run, the frame-rate guard's
// go-ahead (BgRateMayStep), BgThinN and the spark shares.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/budget_sim.h"
#include "budget_ref355.h"  // v3.56 PW356

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const unsigned kAll = (1u << BGL_N) - 1u;
static const unsigned kClassic = ((1u << BGL_N) - 1u) & ~BG_HR_LEVELS;  // v3.56 PW356: v3.55's levels (no spark level)
// Echo x1, After Shock Long, Death Presence Strong (the user's screenshot): no 2nd / 3rd shadow to thin
static const unsigned kUser = 1u | 1u << BGL_AI5 | 1u << BGL_ECHOALL | 1u << BGL_AI3DP | 1u << BGL_DP50 | 1u << BGL_ECHO1;
// v3.56 PW356: the steps a level is from 0 among v3.55's levels (the cost models save by steps, not by level numbers)
static int Rank(int L)
{
    int l, r = 0;
    for (l = 1; l <= L && l <= BG_MAX; ++l) r += (kClassic >> l & 1u) != 0;
    return r;
}

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
    float save = 0.15f * (float)Rank(L);  // v3.56 PW356: by steps (v3.55: by the level number, the same then)
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
    Run(&g, fps, 60.0f, kAll, CostLight, FrameOk, NULL, &maxL, &ch, NULL);  // (v3.56 PW356: every level, the spark ones too)
    CHECK(maxL == 0 && ch == 0, "fps %d: a light scene never thins (max %d, %d changes)", fps, maxL, ch);
}

static void TestFight(int fps, float *firstOut, float *backOut, int *maxOut)
{
    BgGov g;
    float first, back;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kClassic, CostFight, FrameFight, &first, &maxL, &ch, &back);  // v3.56 PW356: v3.55's levels
    CHECK(first >= 2.0f + BG_DOWN_S - 0.05f && first <= 2.0f + 1.25f, "fps %d: the first step down at %.2f s (the fight from 2 s; want ~0.8 .. 1.25 s into it)", fps, first);
    CHECK(Rank(maxL) >= 3 && Rank(maxL) <= 6, "fps %d: thinned enough to be on time again (max level %d, %d steps)", fps, maxL, Rank(maxL));
    CHECK(g.level == 0, "fps %d: everything back at the end (level %d)", fps, g.level);
    CHECK(back > 20.0f && back < 20.0f + (BG_UP_S + 0.5f) * (float)Rank(maxL) + 4.0f, "fps %d: back to full at %.1f s (the fight ends at 20 s, %d levels)", fps, back, maxL);
    CHECK(ch <= 2 * Rank(maxL) + 6, "fps %d: no flicker (%d changes for max level %d)", fps, ch, maxL);
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
        steps += BgGovStep(&g, ms / 1000.0f, ms, c, ft, BG_T60_MS, kAll, skip) != 0;  // (v3.56 PW356: every level)
        t += ms / 1000.0f;
    }
    CHECK(steps == 0 && g.level == 0, "fps %d: spikes, a load and a 0.25 s burst do not trip it (%d steps, level %d)", fps, steps, g.level);
}

// a scene right at the edge: level 0 runs late, level 1 is on time with the cost low; each step up is taken back: the
// back-off must grow (fewer and fewer tries), not flicker every few seconds
static float CostEdge(float t, int L, float ft) { (void)t; return (L == 0 ? 10.0f : 5.0f) * ft / 16.667f; }  // (L 0 or not: no level number)
static float FrameEdge(float t, int L, float ft, float c) { (void)t, (void)c; return L == 0 ? 1.6f * ft : ft; }
static void TestRebound(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 180.0f, kClassic, CostEdge, FrameEdge, NULL, &maxL, &ch, NULL);  // v3.56 PW356: v3.55's levels
    CHECK(ch <= 2 * 12, "fps %d: at most ~12 tries in 3 minutes at the edge (%d changes)", fps, ch);
    CHECK(g.backoff >= BG_UP_MAX - 1e-3f, "fps %d: the back-off grew to its cap (%.1f s)", fps, g.backoff);
    CHECK(maxL == BgNextUp(kClassic, 0), "fps %d: one level is enough (max %d)", fps, maxL);
}

// frames late while the effects cost little (the game's own heavy room, the driver): thinning would not help, so it does not
// v3.56 PW356: below a high rate (30 / 60 fps here); at 120 / 180 a blind try is made and taken back (TestHrFutile)
static float CostTiny(float t, int L, float ft) { (void)t, (void)L; return 1.2f * ft / 16.667f; }
static float FrameHalfLate(float t, int L, float ft, float c) { (void)L, (void)c; return ((int)(t * 30.0f) & 1) ? 1.8f * ft : ft; }
static void TestNotOurs(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kAll, CostTiny, FrameHalfLate, NULL, &maxL, &ch, NULL);
    if (!BgHighRate(1000.0f / (float)fps))
        CHECK(maxL == 0, "fps %d: late frames that are not the effects' doing thin nothing (max %d)", fps, maxL);
    else  // v3.56 PW356
        CHECK(g.blindSteps >= 1 && g.takeBacks >= g.blindSteps - 1 && g.blindKept == 0 && maxL <= BGL_SPARK1,
              "fps %d: a high rate tries blind, takes it back (steps %d, taken back %d, kept %d, max %d)", fps, g.blindSteps, g.takeBacks,
              g.blindKept, maxL);
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


// ================================================================ v3.56 PW356: the high-rate fallback
// a scripted frame stream: one step of the governor under test (and of v3.55's), its result compared
typedef struct { unsigned seed; } Lcg;
static float Rnd(Lcg *r) { r->seed = r->seed * 1664525u + 1013904223u; return (float)(r->seed >> 8) * (1.0f / 16777216.0f); }
static int MapOld(int l) { return l == 0 ? 0 : l + 2; }  // v3.55 level -> v3.56 level
static unsigned MapMask(unsigned old)
{
    unsigned u = old & 1u;
    int l;
    for (l = 1; l <= REF_MAX; ++l)
        if (old >> l & 1u) u |= 1u << (l + 2);
    return u;
}
// v3.55's governor and v3.56's on the same random streams: below 115 fps (and at any rate with hrOff) the same steps at
// the same frames, levels mapped 1..9 -> 3..11, with the spark bits set in the new mask (they must be ignored)
static void TestRef355(void)
{
    static const int kFps[6] = { 24, 30, 50, 60, 90, 110 };
    static const int kHiFps[3] = { 120, 144, 180 };
    int run, mism = 0, frames = 0, steps = 0;
    for (run = 0; run < 72; ++run) {
        const int hrOff = run >= 54;
        const int fps = hrOff ? kHiFps[run % 3] : kFps[run % 6];
        const float ft = 1000.0f / (float)fps;
        Lcg r;
        BgGov g;
        RefGov h;
        unsigned oldMask;
        float t = 0.0f, phaseEnd = 0.0f, cost = 0.0f, lateP = 0.0f, lateK = 1.0f;
        int f;
        r.seed = 0x9E3779B9u * (unsigned)(run + 1);
        oldMask = 1u | ((unsigned)(Rnd(&r) * 1024.0f) & 0x3FEu);
        BgGovReset(&g);
        RefGovReset(&h);
        g.hrOff = hrOff;
        for (f = 0; f < fps * 150 && t < 150.0f; ++f) {
            float ms, c;
            int skip, a, b;
            if (t >= phaseEnd) {  // a new phase: 0.2 .. 12 s of some cost and some share of late frames
                phaseEnd = t + 0.2f + 12.0f * Rnd(&r) * Rnd(&r);
                cost = (Rnd(&r) < 0.3f ? 0.0f : 12.0f * Rnd(&r) * Rnd(&r)) * ft / 16.667f;
                lateP = Rnd(&r) < 0.4f ? 0.0f : Rnd(&r);
                lateK = 1.0f + 1.5f * Rnd(&r);
            }
            ms = Rnd(&r) < lateP ? lateK * ft : ft * (0.97f + 0.06f * Rnd(&r));
            if (Rnd(&r) < 0.002f) ms = 60.0f + 300.0f * Rnd(&r);  // hitches
            c = cost * (0.8f + 0.4f * Rnd(&r)) * (1.0f - 0.08f * (float)h.level);
            skip = Rnd(&r) < 0.01f;
            a = RefGovStep(&h, ms / 1000.0f, ms, c, ft, BG_T60_MS, oldMask, skip);
            b = BgGovStep(&g, ms / 1000.0f, ms, c, ft, BG_T60_MS, MapMask(oldMask) | BG_HR_LEVELS, skip);
            ++frames;
            steps += a != 0;
            if (a != b || g.level != MapOld(h.level)) {
                if (mism < 5) printf("  run %d (%d fps%s) frame %d: v3.55 %+d -> level %d, v3.56 %+d -> level %d\n", run, fps,
                                     hrOff ? ", hrOff" : "", f, a, h.level, b, g.level);
                ++mism;
            }
            t += ms / 1000.0f;
        }
    }
    CHECK(mism == 0, "below 115 fps and with the high-rate rules off: the v3.55 governor's steps exactly (%d mismatches in %d frames, %d steps)",
          mism, frames, steps);
    CHECK(steps > 200, "the streams step often enough to mean something (%d steps)", steps);
    printf("v3.55 reference: %d frames, %d steps, %d mismatches (24 .. 110 fps, and 120 / 144 / 180 fps with hrOff)\n", frames, steps, mism);
}

// a GPU-bound fight at a high rate: the effects cost 0.3 ms of CPU (no evidence), each step of thinning makes the frames
// 6 % of the target shorter until they are on time
static float CostGpu(float t, int L, float ft) { (void)t, (void)L, (void)ft; return 0.3f; }
static int Steps(int L)  // steps taken from 0 over every level
{
    int l, s = 0;
    for (l = 1; l <= L; ++l) ++s;
    return s;
}
static float FrameGpu(float t, int L, float ft, float c)
{
    float k = 1.20f - 0.06f * (float)Steps(L);
    (void)t, (void)c;
    if (k < 0.99f) k = 0.99f;
    return k * ft;
}
static void TestHrBlindHelps(int fps)
{
    BgGov g;
    float first;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 60.0f, kAll, CostGpu, FrameGpu, &first, &maxL, &ch, NULL);
    CHECK(first >= BG_DOWN_S - 0.05f && first <= 1.3f, "fps %d: GPU-bound: the first blind step at %.2f s", fps, first);
    CHECK(g.blindSteps >= 3 && g.blindKept >= 3 && g.takeBacks == 0, "fps %d: every blind step helps and is kept (%d steps, %d kept, %d back)",
          fps, g.blindSteps, g.blindKept, g.takeBacks);
    CHECK(maxL == 3, "fps %d: thinned to on time in 3 steps (max level %d: frames %.2f x the target there)", fps, maxL,
          FrameGpu(0, maxL, 1.0f, 0));
    CHECK(maxL >= BGL_SPARK2, "fps %d: the spark levels first", fps);
    CHECK(ch <= 2 * maxL + 20, "fps %d: no flicker (%d changes)", fps, ch);
    printf("high rate %d fps, GPU-bound: first blind step %.2f s, %d blind step(s), %d kept, %d taken back, deepest %d, %d changes in 60 s\n",
           fps, first, g.blindSteps, g.blindKept, g.takeBacks, maxL, ch);
}
// late frames that thinning cannot help (the game's scene, a vsync below the rate): blind tries taken back, the hold-off
// doubling; the frame-rate guard is told it may act
static float FrameStuck(float t, int L, float ft, float c) { (void)t, (void)L, (void)c; return 1.15f * ft; }
static void TestHrFutile(int fps)
{
    const float ft = 1000.0f / (float)fps;
    BgGov g;
    float t = 0.0f, thinT = 0.0f, yieldT = 0.0f, firstYield = -1.0f;
    BgGovReset(&g);
    while (t < 180.0f) {
        const float ms = FrameStuck(t, g.level, ft, 0.0f);
        BgGovStep(&g, ms / 1000.0f, ms, 0.4f, ft, BG_T60_MS, kAll, 0);
        if (g.level > 0) thinT += ms / 1000.0f;
        if (BgRateMayStep(&g)) {
            yieldT += ms / 1000.0f;
            if (firstYield < 0.0f) firstYield = t;
        }
        t += ms / 1000.0f;
    }
    CHECK(g.blindSteps >= 3 && g.blindSteps <= 5, "fps %d: a few blind tries in 3 minutes (%d: 20 / 40 / 80 / 120 s hold-offs)", fps, g.blindSteps);
    CHECK(g.blindKept == 0 && g.takeBacks >= g.blindSteps - g.blindPend, "fps %d: every one taken back (%d of %d)", fps, g.takeBacks, g.blindSteps);
    CHECK(thinT < 0.06f * 180.0f, "fps %d: thinned %.1f s of 180 (only the tries)", fps, thinT);
    CHECK(g.blindRetry >= BG_HR_RETRY_MAX - 1e-3f, "fps %d: the hold-off grew to its cap (next %.0f s)", fps, g.blindRetry);
    CHECK(firstYield > 0.0f && firstYield < 6.0f && yieldT > 0.85f * 180.0f, "fps %d: the frame-rate guard may act from %.1f s, %.0f s of 180",
          fps, firstYield, yieldT);
    printf("high rate %d fps, stuck: %d blind tries in 180 s, %d taken back, thinned %.1f s, the frame-rate guard may act from %.1f s (%.0f s)\n",
           fps, g.blindSteps, g.takeBacks, thinT, firstYield, yieldT);
    BgGovRoomNew(&g);
    CHECK(!(g.blindHold > 0.0f) && g.blindRetry == BG_HR_RETRY, "a new room: a blind try may come at once, the hold-off from 20 s");
}
// no power drawing (no work at all): nothing is stepped; the frame-rate guard may act after BG_YIELD_S
static void TestHrNoPower(int fps)
{
    const float ft = 1000.0f / (float)fps;
    BgGov g;
    float t = 0.0f, firstYield = -1.0f;
    int steps = 0;
    BgGovReset(&g);
    while (t < 30.0f) {
        const float ms = 1.3f * ft;
        steps += BgGovStep(&g, ms / 1000.0f, ms, 0.0f, ft, BG_T60_MS, kAll, 0) != 0;
        if (firstYield < 0.0f && BgRateMayStep(&g)) firstYield = t;
        t += ms / 1000.0f;
    }
    CHECK(steps == 0 && g.level == 0, "fps %d: no power drawing: no step (%d)", fps, steps);
    CHECK(firstYield >= BG_YIELD_S - 0.05f && firstYield < BG_YIELD_S + 1.0f, "fps %d: the frame-rate guard may act from %.2f s", fps, firstYield);
}
// a frame limiter's jitter at a high rate: 20 % of the frames 4 % long, 7 % 7 % long (late), on average on time: no step,
// and a thinned level comes back (calm under 10 % late)
static void TestHrJitter(int fps)
{
    const float ft = 1000.0f / (float)fps;
    BgGov g;
    Lcg r;
    float t = 0.0f;
    int steps = 0, ups = 0;
    r.seed = 77u + (unsigned)fps;
    BgGovReset(&g);
    while (t < 60.0f) {
        const float u = Rnd(&r), ms = u < 0.07f ? 1.07f * ft : u < 0.27f ? 1.04f * ft : 0.985f * ft;
        steps += BgGovStep(&g, ms / 1000.0f, ms, 0.8f, ft, BG_T60_MS, kAll, 0) != 0;
        t += ms / 1000.0f;
    }
    CHECK(steps == 0, "fps %d: jitter is not a miss (%d steps)", fps, steps);
    BgGovReset(&g);
    g.level = BGL_SPARK2;
    g.holdT = 100.0f;
    t = 0.0f;
    while (t < 20.0f) {
        const float u = Rnd(&r), ms = u < 0.07f ? 1.07f * ft : 0.99f * ft;
        ups += BgGovStep(&g, ms / 1000.0f, ms, 0.8f, ft, BG_T60_MS, kAll, 0) < 0;
        t += ms / 1000.0f;
    }
    CHECK(g.level == 0 && ups == 2, "fps %d: with 7 %% late frames a thinned level comes back (level %d, %d up)", fps, g.level, ups);
}
// the effects' measured work is evidence at a high rate: no blind step, the late rule 1.06 x
static float CostCpu(float t, int L, float ft) { (void)t; return (2.2f - 0.25f * (float)Steps(L)) * ft / 5.556f; }
static float FrameCpu(float t, int L, float ft, float c) { (void)t, (void)L; return c > 1.0f * ft / 5.556f ? 1.09f * ft : ft; }
static void TestHrEvidence(int fps)
{
    BgGov g;
    int maxL, ch;
    BgGovReset(&g);
    Run(&g, fps, 30.0f, kAll, CostCpu, FrameCpu, NULL, &maxL, &ch, NULL);
    CHECK(g.blindSteps == 0 && maxL >= 3, "fps %d: 9 %% late frames with the effects' work as evidence: evidence steps (blind %d, max %d)", fps,
          g.blindSteps, maxL);
    BgGovReset(&g);
    g.hrOff = 1;
    Run(&g, fps, 30.0f, kAll, CostCpu, FrameCpu, NULL, &maxL, &ch, NULL);
    CHECK(maxL == 0, "fps %d: the same with the high-rate rules off: 9 %% late is not late (v3.55: 1.25 x; max %d)", fps, maxL);
}
// the rate falls from 180 to 60 while a spark level is in force: the sparks are full again at once (BgSparkK), the level
// comes back up when calm
static void TestHrRateDrop(void)
{
    BgGov g;
    float t = 0.0f;
    int f;
    BgGovReset(&g);
    while (t < 20.0f && g.level < BGL_SPARK2) {  // 180 fps, GPU-bound: the blind steps reach SPARK2
        const float ms = FrameGpu(t, g.level, 5.556f, 0.0f);
        BgGovStep(&g, ms / 1000.0f, ms, 0.3f, 5.556f, BG_T60_MS, kAll, 0);
        t += ms / 1000.0f;
    }
    CHECK(g.level >= BGL_SPARK2 && BgSparkK(g.level, 1) < 0.4f, "180 fps: at a spark level (%d)", g.level);
    for (f = 0; f < 60 * 30; ++f) BgGovStep(&g, 1.0f / 60.0f, 16.667f, 0.3f, 16.667f, BG_T60_MS, kAll, 0);
    CHECK(BgSparkK(g.level, g.hr) == 1.0f && !g.hr && !g.blindPend, "60 fps: the sparks full, no high rate, nothing pending");
    CHECK(g.level == 0, "60 fps calm: back to full (%d)", g.level);
}
static void TestHrHelpers(void)
{
    int n, m, k;
    float a;
    CHECK(BgHighRate(1000.0f / 120.0f) && BgHighRate(1000.0f / 115.0f) && BgHighRate(1000.0f / 180.0f) && BgHighRate(1000.0f / 240.0f),
          "120 / 115 / 180 / 240 fps: a high rate");
    CHECK(!BgHighRate(1000.0f / 110.0f) && !BgHighRate(1000.0f / 60.0f) && !BgHighRate(1000.0f / 30.0f) && !BgHighRate(0.0f), "110 / 60 / 30 / 0: not");
    CHECK(BgSparkK(0, 1) == 1.0f && BgSparkK(BGL_SPARK1, 1) == 0.6f && BgSparkK(BGL_SPARK2, 1) == 0.35f && BgSparkK(BG_MAX, 1) == 0.35f,
          "the spark shares at a high rate");
    CHECK(BgSparkK(BGL_SPARK2, 0) == 1.0f && BgSparkK(BG_MAX, 0) == 1.0f, "below a high rate: full sparks at any level");
    CHECK(BgNextUp(kAll & ~BG_HR_LEVELS, 0) == BGL_ECHO3 && BgNextUp(kAll, 0) == BGL_SPARK1, "the ladder starts with the sparks at a high rate");
    CHECK(BgEchoLevel(BGL_SPARK2) == 0 && BgEchoBodies(BGL_SPARK2) == 3 && BgAiCopies(BGL_SPARK2, 8) == 8 && BgDpKeep(BGL_SPARK2) == 1.0f,
          "the spark levels thin no body, copy or puff");
    for (n = 0; n <= 12; ++n)
        for (k = 0; k <= 20; ++k) {
            const float kk = (float)k / 20.0f;
            m = BgThinN(n, kk, 1, &a);
            CHECK(m <= n && (n == 0 ? m == 0 : m >= 1) && a > 0.0f && a <= 1.0f, "BgThinN(%d, %.2f): %d, last %.2f", n, kk, m, a);
            if (n > 0 && kk * (float)n >= 1.0f)
                CHECK(fabsf((float)(m - 1) + a - kk * (float)n) < 1e-3f, "BgThinN(%d, %.2f): %d whole + %.2f = n k", n, kk, m - 1, a);
        }
    CHECK(BgThinN(10, 1.0f, 0, &a) == 10 && a == 1.0f && BgThinN(10, 0.0f, 0, &a) == 0 && BgThinN(5, 0.0f, 2, NULL) == 2, "BgThinN's ends");
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
        if (!BgHighRate(1000.0f / (float)kFps[r]))
            CHECK(fabsf(back[r] - back[1]) <= 1.0f, "fps %d: back to full at %.1f s (60 fps %.1f)", kFps[r], back[r], back[1]);
        else  // v3.56 PW356: calm at a high rate is under 15 % late (v3.55: 5 %): back no later, up to 2.5 s sooner
            CHECK(back[r] <= back[1] + 0.5f && back[r] >= back[1] - 2.5f, "fps %d: back to full at %.1f s (60 fps %.1f)", kFps[r], back[r], back[1]);
    }
    printf("fight: first step %.2f / %.2f / %.2f / %.2f s, deepest %d / %d / %d / %d, back to full %.1f / %.1f / %.1f / %.1f s (30 / 60 / 120 / 180 fps)\n",
           first[0], first[1], first[2], first[3], maxL[0], maxL[1], maxL[2], maxL[3], back[0], back[1], back[2], back[3]);
    TestUseful();
    TestKeep();
    TestPuffs();
    TestRamp();
    TestText();
    TestRef355();  // v3.56 PW356
    TestHrHelpers();
    for (r = 0; r < 3; ++r) {
        static const int kHi[3] = { 120, 144, 180 };
        TestHrBlindHelps(kHi[r]), TestHrFutile(kHi[r]), TestHrNoPower(kHi[r]), TestHrJitter(kHi[r]), TestHrEvidence(kHi[r]);
    }
    TestHrRateDrop();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "budget_sim_test ok", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
