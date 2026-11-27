// v3.58 GX358: native test of the effect budget's quick rules (src\budget_sim.h: the start step, the refill, the quick
// step and go-ahead) together with the rate guard's quick window (src\rateguard_sim.h FrgFrameEx), test\build_gx358_test.cmd.
// USER_FEEDBACK_v357 item 13 ("Powers on simultaneously slowed down the game for a few seconds, but then it sped back up").
// Two cases from the user's log play_v357_2026-11-27.log are replayed as frame-time models:
//   S1 (01:01:45 - 01:02:20, stage 2 room 1): After Shock Long on; in the Options Death Presence Strong and Echo Doubles
//      Shadow Cast x3 are turned on; play resumes (the Options' pause), the fog draws 0.3 s later, the shadows form 0.5 s
//      later. v3.56: the budget 0 -> 11 one level a second (01:02:06.7 .. 01:02:16.8), three of the guard's windows held
//      (7.39 / 8.10 / 6.78 ms against 5.56), on time again at level 10 - 11: about 11 s of late frames.
//   S2 (00:15:35 - 00:15:45, stage 5 room B): After Shock on from before, a heavy room: the frames 8.7 -> 7.4 ms at any
//      level (GPU-bound), the budget 1 -> 2 -> 7 -> 9, nothing left at 9, the guard 180 -> 120 after its third 3 s window.
// The frame of a level is the log's mean there (the change eased over 0.5 s, as the effects fade what they thin), +-0.5 ms
// of jitter; the frames at 120 take as long as at 180 (the work per frame is the same). "Slow" = a 250 ms stretch of real
// time whose mean frame is over 1.06 x the frame target in force (the game runs 6 % or more slow there).
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/budget_sim.h"
#include "../src/rateguard_sim.h"

static int g_fail, g_n, g_trace;  // gx358_test.exe trace: each level change of the cases is printed
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { unsigned s; } Lcg;
static float Rnd(Lcg *r) { r->s = r->s * 1664525u + 1013904223u; return (float)(r->s >> 8) / 16777216.0f; }

// the useful levels (budget.inc BgUseful at a high rate): S1 Echo x3 + After Shock Long + Death Presence = every level;
// S2 After Shock Long alone = the sparks, After Shock 8 -> 5 and 3 copies
#define U_ALL   0xFFFu
#define U_AI    (1u | 1u << BGL_SPARK1 | 1u << BGL_SPARK2 | 1u << BGL_AI5 | 1u << BGL_AI3DP)
static const float kS1Frame[BGL_N] = { 7.6f, 7.6f, 7.5f, 7.6f, 7.9f, 8.1f, 7.9f, 7.4f, 7.0f, 6.6f, 5.8f, 5.45f };
static const float kS1Cost[BGL_N] = { 3.3f, 3.3f, 3.9f, 4.8f, 5.0f, 5.0f, 4.9f, 4.4f, 4.1f, 3.1f, 3.2f, 2.1f };
static const float kS2Frame[BGL_N] = { 8.9f, 8.8f, 8.6f, 8.6f, 8.6f, 8.6f, 8.6f, 8.2f, 8.2f, 7.6f, 7.6f, 7.6f };
static const float kS2Cost[BGL_N] = { 3.0f, 3.0f, 3.6f, 3.6f, 3.6f, 3.6f, 3.6f, 2.3f, 2.3f, 2.0f, 2.0f, 2.0f };
static const float kLightFrame[BGL_N] = { 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f };

typedef struct {
    const float *frame, *cost;
    unsigned useful;
    int startLevel;
    int start;        // 1 = S1: a pause, then Death Presence at +0.3 s and Echo at +0.5 s start beside After Shock
    float secs;
    int quick;        // the v3.58 rules (0 = v3.56)
    unsigned seed;
} Case;
typedef struct {
    float slowS, slowAfterGrace, longest, guardAt, lastSlowAt;
    int deepest, preSteps, preBacks, quickSteps, cap, endLevel, downs, ups;
} Res;

// the frame-rate guard's go-ahead as rateguard.inc reads it (FrgShedding: a step down within the last window and the
// budget still able to shed; fast: the budget's quick go-ahead)
static Res RunCase(const Case *c)
{
    BgGov bg;
    FrgState fg;
    Lcg r;
    Res o;
    float t = -10.0f, shownMs = c->frame[c->startLevel], fromMs = shownMs, easeT = 1.0f, lastShedAt = -100.0f;
    float bucketT = 0.0f, bucketSum = 0.0f, slowRun = 0.0f;
    int bucketN = 0, lvWas, f, skipN = 0;
    memset(&o, 0, sizeof o);
    o.guardAt = -1.0f;
    r.s = c->seed;
    BgGovReset(&bg);
    FrgReset(&fg, 180);
    bg.quick = c->quick;
    bg.level = c->startLevel;
    lvWas = bg.level;
    o.deepest = bg.level;
    // before t = 0: 10 s of play with After Shock alone (S1: frames on time, its cost small) or the room before (S2)
    for (f = 0; t < 0.0f; ++f) {
        const float ms = 5.3f + (Rnd(&r) - 0.5f) * 0.6f;
        const unsigned mask = 1u << BG_PW_AI;
        BgGovPowers(&bg, mask, ms / 1000.0f, 1, c->start ? U_AI : c->useful);
        if (c->start) BgGovStep(&bg, ms / 1000.0f, ms, 0.8f, 1000.0f / 180.0f, BG_T60_MS, U_AI, 0);
        FrgFrameEx(&fg, ms, 180, 0, 0);
        t += ms / 1000.0f;
    }
    if (!c->start) bg.level = c->startLevel, lvWas = bg.level, bg.holdT = 100.0f;
    // t = 0: S1 the Options' pause ends (the guard's break; the budget skips 20 frames), S2 the room change (1.5 s skip,
    // the guard's room change); the start's powers then draw
    if (c->start) FrgBreak(&fg), skipN = 20;
    else FrgRoomChange(&fg);
    t = 0.0f;
    while (t < c->secs) {
        const int rate = FrgRate(&fg);
        const float target = 1000.0f / (float)rate;
        const int drawDp = !c->start || t >= 0.3f, drawEcho = !c->start || t >= 0.5f;
        const unsigned mask = 1u << BG_PW_AI | (c->start && drawDp ? 1u << BG_PW_DP : 0u) | (c->start && drawEcho ? 1u << BG_PW_ECHO : 0u);
        const float load = c->start ? (drawEcho ? 1.0f : drawDp ? 0.5f : 0.0f) : 1.0f;
        const unsigned useful = c->start ? (drawEcho ? c->useful : drawDp ? (U_AI | 1u << BGL_AI3DP | 1u << BGL_DP50) : U_AI) : c->useful;
        const int skip = c->start ? skipN > 0 : t < 1.5f;
        float ms, cost, goal;
        int step;
        if (bg.level != lvWas && g_trace)
            printf("    t %6.2f s: level %2d -> %2d (kind %d, refill %d, late %.2f, cost %.2f, frame %.2f ms, rate %d)\n", t, lvWas, bg.level,
                   bg.stepKind, bg.pre, bg.lateE, bg.costE, bg.frameE, rate);
        if (bg.level != lvWas) fromMs = shownMs, easeT = 0.0f, lvWas = bg.level;  // the level changed: its look eases in
        easeT += shownMs / 1000.0f;
        goal = 5.3f + (c->frame[bg.level] - 5.3f) * load;
        shownMs = easeT >= 0.5f ? goal : fromMs + (goal - fromMs) * (easeT / 0.5f);
        ms = shownMs + (Rnd(&r) - 0.5f) * 1.0f;
        cost = c->cost[bg.level] * load;
        if (skipN > 0) --skipN;
        if (BgGovPowers(&bg, mask, ms / 1000.0f, BgHighRate(target), useful) > 0) lastShedAt = t;
        step = BgGovStep(&bg, ms / 1000.0f, ms, cost, target, BG_T60_MS, useful, skip);
        if (step > 0) lastShedAt = t;
        {
            const int shedding = t - lastShedAt < FRG_WIN_MS / 1000.0f && !BgRateMayStep(&bg);
            const int fast = bg.quick && BgRateMayStep(&bg);
            if (FrgFrameEx(&fg, ms, rate, t < 1.5f && !c->start ? 0 : shedding, c->quick && fast) == FRG_EV_DOWN && o.guardAt < 0.0f)
                o.guardAt = t;
        }
        if (bg.level > o.deepest) o.deepest = bg.level;
        bucketT += ms / 1000.0f, bucketSum += ms, ++bucketN;
        if (bucketT >= 0.25f) {
            const int slow = bucketSum / (float)bucketN > 1.06f * target;
            if (slow) {
                o.slowS += bucketT, slowRun += bucketT, o.lastSlowAt = t;
                if (t > 1.5f) o.slowAfterGrace += bucketT;
                if (slowRun > o.longest) o.longest = slowRun;
            } else slowRun = 0.0f;
            bucketT = bucketSum = 0.0f, bucketN = 0;
        }
        t += ms / 1000.0f;
    }
    o.preSteps = bg.preSteps, o.preBacks = bg.preBacks, o.quickSteps = bg.quickSteps, o.cap = fg.cap, o.endLevel = bg.level;
    o.downs = bg.downs, o.ups = bg.ups;
    return o;
}
static void Say(const char *what, const Res *o)
{
    printf("%-44s slow %.2f s (after the grace %.2f, longest %.2f, last at %.1f s); guard %s%.2f s, cap %d; deepest %d, end %d; "
           "start steps %d, refill take-backs %d, quick steps %d; %d down / %d up\n", what, o->slowS, o->slowAfterGrace, o->longest,
           o->lastSlowAt, o->guardAt < 0.0f ? "never " : "at ", o->guardAt < 0.0f ? 0.0f : o->guardAt, o->cap, o->deepest, o->endLevel,
           o->preSteps, o->preBacks, o->quickSteps, o->downs, o->ups);
}

static void TestS1(void)
{
    Case c = { kS1Frame, kS1Cost, U_ALL, 0, 1, 40.0f, 0, 1234u };
    Res v356, v358;
    unsigned seed;
    float worst = 0.0f;
    v356 = RunCase(&c);
    Say("S1 three powers at 180, v3.56's rules:", &v356);
    CHECK(v356.slowS >= 7.0f && v356.longest >= 7.0f, "the model shows the log's few slow seconds with v3.56's rules (%.1f s, the log ~11 s)", v356.slowS);
    CHECK(v356.deepest >= 10 && v356.preSteps == 0, "v3.56: down to level %d one step at a time, no start step", v356.deepest);
    c.quick = 1;
    v358 = RunCase(&c);
    Say("S1 three powers at 180, v3.58's quick rules:", &v358);
    CHECK(v358.preSteps == 1 && v358.deepest == BG_MAX, "one start step, to the deepest level (%d steps, deepest %d)", v358.preSteps, v358.deepest);
    CHECK(v358.slowS <= 1.1f && v358.longest <= 1.0f, "the acceptance: about 1 s of late frames at most (%.2f s; longest %.2f; from the fog's start at 0.3 s to the start level shown)", v358.slowS, v358.longest);
    CHECK(v358.cap == 0, "180 is held (the guard did not step: the budget was enough)");
    CHECK(v358.endLevel >= 10, "the budget settles where the log's did (level %d; 10 - 11)", v358.endLevel);
    for (seed = 1; seed <= 40; ++seed) {  // other jitter
        Res o;
        c.seed = seed * 2654435761u;
        o = RunCase(&c);
        if (o.slowS > worst) worst = o.slowS;
        CHECK(o.slowS <= 1.1f && o.preSteps == 1, "seed %u: %.2f s slow", seed, o.slowS);
    }
    printf("S1 over 40 jitter seeds: the most slow time %.2f s\n", worst);
}
static void TestS2(void)
{
    Case c = { kS2Frame, kS2Cost, U_AI, BGL_SPARK1, 0, 20.0f, 0, 99u };
    Res v356, v358;
    v356 = RunCase(&c);
    Say("S2 heavy room at 180, v3.56's rules:", &v356);
    CHECK(v356.cap == 120 && v356.guardAt > 7.0f, "v3.56: 180 -> 120 after its 3 s windows (at %.1f s; the log 10.2 s)", v356.guardAt);
    c.quick = 1;
    v358 = RunCase(&c);
    Say("S2 heavy room at 180, v3.58's quick rules:", &v358);
    CHECK(v358.cap == 120 && v358.guardAt > 1.5f && v358.guardAt < 3.5f, "the quick window: 180 -> 120 at %.2f s (the room's 1.5 s grace first)", v358.guardAt);
    CHECK(v358.slowAfterGrace <= 2.0f && v358.slowS < 0.5f * v356.slowS, "slow %.2f s after the grace (v3.56 %.2f s in all)", v358.slowAfterGrace, v356.slowS);
    CHECK(v358.preSteps == 0 && v358.quickSteps >= 2, "no start (After Shock drew before), quick steps %d", v358.quickSteps);
}
// a PC that holds 180 with every power: the start step costs nothing but a short thinning, everything comes back
static void TestLightPc(void)
{
    Case c = { kLightFrame, kS1Cost, U_ALL, 0, 1, 20.0f, 1, 7u };
    Res o = RunCase(&c);
    Say("light PC, three powers at 180, v3.58:", &o);
    CHECK(o.preSteps == 1 && o.slowS == 0.0f && o.cap == 0, "a start step, no slow frame, 180 kept");
    CHECK(o.endLevel == 0 && o.ups == BG_MAX, "the refill brings every level back (end %d, %d up)", o.endLevel, o.ups);
}
// what starts a power and what does not
static void TestStarts(void)
{
    BgGov g;
    int f, steps = 0;
    const float ft = 1000.0f / 180.0f;
    BgGovReset(&g);
    g.quick = 1;
    for (f = 0; f < 180 * 5; ++f) steps += BgGovPowers(&g, 1u << BG_PW_ECHO | 1u << BG_PW_AI, ft / 1000.0f, 1, U_ALL);
    CHECK(steps == 1 && g.level == BG_MAX && g.pre, "two powers drawing from the start of play: one start step (%d)", steps);
    // a door, a pause, a cut-scene: no play frame, the powers draw again after it: no start
    BgGovReset(&g);
    g.quick = 1;
    for (f = 0; f < 180 * 20; ++f) BgGovPowers(&g, 1u << BG_PW_ECHO | 1u << BG_PW_DP, ft / 1000.0f, 1, U_ALL);
    g.level = 0, g.pre = 0;
    for (f = 0; f < 180 * 2; ++f) BgGovPowers(&g, 1u << BG_PW_ECHO | 1u << BG_PW_DP, ft / 1000.0f, 1, U_ALL);
    CHECK(g.level == 0, "powers that kept drawing: no step later");
    for (f = 0; f < 60; ++f) BgGovPowers(&g, 0u, ft / 1000.0f, 1, U_ALL);  // a room's first frames without them (0.33 s)
    steps = 0;
    for (f = 0; f < 180; ++f) steps += BgGovPowers(&g, 1u << BG_PW_ECHO | 1u << BG_PW_DP, ft / 1000.0f, 1, U_ALL);
    CHECK(steps == 0 && g.level == 0, "a short gap (0.3 s): no start");
    // off for 6 s of play, then two start together: a step; a single one: none
    for (f = 0; f < 180 * 6; ++f) BgGovPowers(&g, 1u << BG_PW_AI, ft / 1000.0f, 1, U_ALL);
    steps = 0;
    for (f = 0; f < 180; ++f) steps += BgGovPowers(&g, 1u << BG_PW_AI | 1u << BG_PW_ECHO, ft / 1000.0f, 1, U_ALL);
    CHECK(steps == 0, "one power starting beside one that drew: no step");
    for (f = 0; f < 180 * 6; ++f) BgGovPowers(&g, 1u << BG_PW_AI, ft / 1000.0f, 1, U_ALL);
    steps = 0;
    for (f = 0; f < 90; ++f) steps += BgGovPowers(&g, 1u << BG_PW_AI | 1u << BG_PW_ECHO, ft / 1000.0f, 1, U_ALL);
    for (f = 0; f < 90; ++f) steps += BgGovPowers(&g, 1u << BG_PW_AI | 1u << BG_PW_ECHO | 1u << BG_PW_GT, ft / 1000.0f, 1, U_ALL);
    CHECK(steps == 1 && g.preMask == (1u << BG_PW_ECHO | 1u << BG_PW_GT), "Echo, then Ghost Teleport's fade 0.5 s later: one start step (%d, mask %X)", steps, g.preMask);
    // not at 60 fps, not with the quick rules off, not with the high-rate rules off
    BgGovReset(&g);
    g.quick = 1;
    for (f = 0; f < 60; ++f) steps = BgGovPowers(&g, 3u, 1.0f / 60.0f, 0, U_ALL);
    CHECK(g.level == 0, "60 fps: no start step");
    BgGovReset(&g);
    for (f = 0; f < 180; ++f) BgGovPowers(&g, 3u, ft / 1000.0f, 1, U_ALL);
    CHECK(g.level == 0, "quick off (v3.56): no start step");
    BgGovReset(&g);
    g.quick = 1, g.hrOff = 1;
    for (f = 0; f < 180; ++f) BgGovPowers(&g, 3u, ft / 1000.0f, 1, U_ALL);
    CHECK(g.level == 0, "EffectBudgetHighRate=0: no start step");
    // already deeper than the start level: nothing
    BgGovReset(&g);
    g.quick = 1, g.level = BGL_AI3DP;
    for (f = 0; f < 180; ++f) BgGovPowers(&g, 3u, ft / 1000.0f, 1, U_AI);
    CHECK(g.level == BGL_AI3DP && !g.pre, "the level already the deepest useful one: no step");
    CHECK(BgPowerMask(0.02f, 0.5f, 0.0f, 0) == 1u << BG_PW_AI && BgPowerMask(1.0f, 0.0f, 0.04f, 1) == 0xDu, "the mask from the ledger");
    CHECK(BgTopUseful(U_AI) == BGL_AI3DP && BgTopUseful(1u) == 0 && BgTopUseful(U_ALL) == BG_MAX, "the deepest useful level");
}
// the refill: a level the frames cannot hold comes back at once and v3.56's back-off takes over (no flicker)
static void TestRefill(void)
{
    Case c = { kS1Frame, kS1Cost, U_ALL, 0, 1, 120.0f, 1, 4242u };
    Res o = RunCase(&c);
    Say("S1, two minutes:", &o);
    CHECK(o.preBacks == 1, "one take-back ends the refill (%d)", o.preBacks);
    CHECK(o.downs + o.ups <= 16, "no flicker in two minutes (%d down, %d up)", o.downs, o.ups);
    CHECK(o.slowS <= 2.5f, "in two minutes %.2f s slow at most (the later tries of a lighter level, v3.56's back-off)", o.slowS);
}
// the quick window of the guard: only with the budget's go-ahead, never across a break, not on jitter or hitches
static void TestGuardQuick(void)
{
    FrgState g;
    int i, ev, downs;
    float t;
    FrgReset(&g, 180);
    for (i = 0, t = 0.0f, downs = 0; t < 2.6f; ++i) {
        ev = FrgFrameEx(&g, 7.0f, FrgRate(&g), 0, 1);
        t += 0.007f;
        if (ev == FRG_EV_DOWN && !downs++) CHECK(t > 2.49f && t < 2.8f && g.lastFast, "the grace 1.5 s, then 1 s: a quick step at %.2f s", t);
    }
    CHECK(downs == 1 && g.cap == 120, "7 ms at 180 with the go-ahead: 120 (%d)", downs);
    FrgReset(&g, 180);
    for (t = 0.0f; t < 6.0f; t += 0.007f) FrgFrameEx(&g, 7.0f, FrgRate(&g), 0, 0);
    CHECK(g.cap == 0, "without the go-ahead the 3 s windows decide: no step in 6 s");
    FrgReset(&g, 180);
    for (t = 0.0f, downs = 0; t < 30.0f; t += 0.0064f) downs += FrgFrameEx(&g, 1000.0f / 180.0f * 1.149f, FrgRate(&g), 0, 1) == FRG_EV_DOWN;
    CHECK(downs == 0, "1.149 x: no quick step");
    FrgReset(&g, 180);
    for (t = 0.0f, downs = 0; t < 30.0f; t += 0.007f) {  // a break every 0.9 s (a capture, the game out of front): never a full second
        downs += FrgFrameEx(&g, 7.0f, FrgRate(&g), 0, 1) == FRG_EV_DOWN;
        if (fmodf(t, 0.9f) < 0.007f) FrgBreak(&g), g.graceMs = 0.0f;
    }
    CHECK(downs == 0 && g.cap == 0, "breaks drop the quick window: no step (%d)", downs);
    FrgReset(&g, 180);
    {
        Lcg r;
        r.s = 5u;
        for (t = 0.0f, downs = 0; t < 60.0f;) {  // a limiter's jitter: 10 % of the frames 1.5 x, the rest a hair short
            const float ms = Rnd(&r) < 0.1f ? 8.33f : 5.25f;
            downs += FrgFrameEx(&g, ms, FrgRate(&g), 0, 1) == FRG_EV_DOWN;
            t += ms / 1000.0f;
        }
    }
    CHECK(downs == 0, "jitter around the target with the go-ahead: no step");
    FrgReset(&g, 180);
    for (i = 0, downs = 0; i < 20000; ++i) downs += FrgFrameEx(&g, (i % 180) == 0 ? 400.0f : 5.556f, FrgRate(&g), 0, 1) == FRG_EV_DOWN;
    CHECK(downs == 0, "hitches (400 ms) left out of the quick window too");
    FrgReset(&g, 120);
    for (t = 0.0f, downs = 0; t < 10.0f; t += 0.0105f) downs += FrgFrameEx(&g, 10.5f, FrgRate(&g), 0, 1) == FRG_EV_DOWN;
    CHECK(downs == 1 && g.cap == 60, "120 -> 60 the same way");
    FrgReset(&g, 180);
    for (t = 0.0f; t < 3.0f; t += 0.007f) FrgFrameEx(&g, 7.0f, FrgRate(&g), 0, 1);
    CHECK(g.cap == 120 && FrgRoomChange(&g) == 0 && g.cap == 120, "a quick step is v3.56's cap: kept over a room change");
}
// the quick go-ahead: nothing left to shed for 0.5 s when the frames run far late (v3.56: 1 s), 1 s when mildly late
static void TestYield(void)
{
    BgGov g;
    float t, first;
    const float ft = 1000.0f / 180.0f;
    BgGovReset(&g);
    g.quick = 1;
    for (t = 0.0f, first = -1.0f; t < 5.0f; t += 1.3f * ft / 1000.0f) {
        BgGovStep(&g, 1.3f * ft / 1000.0f, 1.3f * ft, 0.0f, ft, BG_T60_MS, U_ALL, 0);
        if (first < 0.0f && BgRateMayStep(&g)) first = t;
    }
    CHECK(first > 0.4f && first < 0.9f, "far late, nothing drawing: the go-ahead at %.2f s (v3.56 1 s)", first);
    BgGovReset(&g);
    g.quick = 1;
    {
        Lcg r;
        r.s = 3u;
        for (t = 0.0f, first = -1.0f; t < 10.0f;) {  // 40 % of the frames late: not far late
            const float ms = Rnd(&r) < 0.4f ? 1.3f * ft : ft;
            BgGovStep(&g, ms / 1000.0f, ms, 0.0f, ft, BG_T60_MS, U_ALL, 0);
            if (first < 0.0f && BgRateMayStep(&g)) first = t;
            t += ms / 1000.0f;
        }
    }
    CHECK(first >= BG_YIELD_S - 0.05f && first < BG_YIELD_S + 1.0f, "mildly late: the go-ahead after v3.56's %.1f s (%.2f s)", BG_YIELD_S, first);
}

int main(int argc, char **argv)
{
    g_trace = argc > 1 && !strcmp(argv[1], "trace");
    TestS1();
    TestS2();
    TestLightPc();
    TestStarts();
    TestRefill();
    TestGuardQuick();
    TestYield();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "gx358_test ok", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
