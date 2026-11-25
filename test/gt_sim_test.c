// v3.43 GT343 / v3.44 GT344: teleport_sim.h native test (staging/v343_feedback/w2_teleport/PLAN.md,
// staging/v344_feedback/w3_teleport/PLAN.md): Ghost Teleport's trigger (v3.44: Run + Action within 80 ms, either first,
// at 30 / 60 / 120 / 144 Hz; each press used once; a press held from before is none); Richard's fade (0.5 s out, the
// warp, 0.5 s in; called off = back in from where it was; the ghost pre-draw's FIX); the warp ahead (the key, the
// tries); the nearest-first pick (range, height, standing only, at most GT_PICK_MAX); the back direction from the
// model's quaternion (and its fallback); the spot rings; the facing; the clearance from other enemies; the room's
// edges; the hidden keys; the move-held check.
// Build and run: test\build_gt_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/teleport_sim.h"

static int g_fail, g_n, g_verbose;
static void Check(int ok, const char *what)
{
    ++g_n;
    if (ok && g_verbose) printf("ok    %s\n", what);
    if (!ok) {
        printf("FAIL  %s\n", what);
        ++g_fail;
    }
}
static int Near(float a, float b, float eps) { return fabsf(a - b) <= eps; }

// ---- v3.44 GT344: the chord at a tick rate: the first button at tick 10, the second `gap` ticks later
static int ChordRun(int hz, int firstIsRun, int gap, int holdTicks, int *atTick)
{
    GtChord c;
    const float dt = 1.0f / (float)hz;
    int k, r, got = GTC_NONE;
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);  // both up: the reset's "held" goes
    *atTick = -1;
    for (k = 0; k < hz * 2; ++k) {
        const int a = k >= 10 && k < 10 + holdTicks, b = k >= 10 + gap && k < 10 + holdTicks;
        r = GtChordStep(&c, firstIsRun ? a : b, firstIsRun ? b : a, dt, 1);
        if (r != GTC_NONE) {
            if (got != GTC_NONE) return -1;  // (more than one chord)
            got = r, *atTick = k;
        }
    }
    return got;
}
static void ChordRates(void)
{
    static const int kHz[4] = { 30, 60, 120, 144 };
    int i, at, r;
    char what[200];
    for (i = 0; i < 4; ++i) {
        const int hz = kHz[i], in = (int)floorf(GT_CHORD_S * (float)hz + 1e-3f), out = in + 1;
        r = ChordRun(hz, 1, 0, hz, &at);
        sprintf_s(what, sizeof what, "%d Hz: Run and Action on one tick: one chord, Action last (taken back), at that tick", hz);
        Check(r == GTC_ACT_LAST && at == 10, what);
        r = ChordRun(hz, 1, in, hz, &at);
        sprintf_s(what, sizeof what, "%d Hz: Run, then Action %d tick(s) later (%.0f ms): a chord, Action last", hz, in, 1000.0f * in / hz);
        Check(r == GTC_ACT_LAST && at == 10 + in, what);
        r = ChordRun(hz, 0, in, hz, &at);
        sprintf_s(what, sizeof what, "%d Hz: Action, then Run %d tick(s) later: a chord, Run last (Action reached the game)", hz, in);
        Check(r == GTC_RUN_LAST && at == 10 + in, what);
        r = ChordRun(hz, 1, out, hz, &at);
        sprintf_s(what, sizeof what, "%d Hz: Run, then Action %.0f ms later: no chord (Action is the game's)", hz, 1000.0f * out / hz);
        Check(r == GTC_NONE, what);
        r = ChordRun(hz, 0, out, hz, &at);
        sprintf_s(what, sizeof what, "%d Hz: Action, then Run %.0f ms later: no chord", hz, 1000.0f * out / hz);
        Check(r == GTC_NONE, what);
    }
}
static void ChordRules(void)
{
    GtChord c;
    const float dt = 1.0f / 60.0f;
    int k, n;
    // running for a while, then Action (a door while running): no chord
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    for (k = 0, n = 0; k < 60; ++k) n += GtChordStep(&c, 1, 0, dt, 1) != GTC_NONE;
    n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 0, "Run held 1 s, then Action (a door while running): no chord");
    // both held from before the reset (the option turned on, a menu closed): no chord until pressed anew
    GtChordReset(&c);
    for (k = 0, n = 0; k < 60; ++k) n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 0, "both held from before: no chord");
    n += GtChordStep(&c, 1, 0, dt, 1) != GTC_NONE;  // Action up and down again, Run held long
    n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 0, "then a new Action press with Run held long: no chord");
    // one chord a press pair: holding both, tapping nothing: one only
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    for (k = 0, n = 0; k < 120; ++k) n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 1, "both held 2 s: one chord");
    // a quick second Action tap right after a chord (both presses used): no second chord
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    n = GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    n += GtChordStep(&c, 1, 0, dt, 1) != GTC_NONE;
    n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 1, "a second Action tap 33 ms after the chord: no second chord (Run's press was used)");
    // v3.44 GT344-CHORD-1: presses across a non-gameplay phase (live = 0) never pair with a fresh one in play
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    GtChordStep(&c, 0, 1, dt, 0);  // Action pressed in a menu
    n = GtChordStep(&c, 0, 1, dt, 0) != GTC_NONE;
    n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;  // held into play, Run pressed on the first play tick
    Check(n == 0, "Action pressed in a menu, held into play, then Run: no chord");
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    GtChordStep(&c, 1, 0, dt, 1);  // Run pressed in play
    GtChordStep(&c, 1, 0, dt, 0);  // a menu opens (no time passes there)
    n = GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;  // back in play, Action at once
    Check(n == 0, "Run pressed in play, a menu, then Action back in play: no chord (Run's press used)");
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    n = GtChordStep(&c, 1, 1, dt, 0) != GTC_NONE;  // both pressed together in a menu
    n += GtChordStep(&c, 1, 1, dt, 1) != GTC_NONE;
    Check(n == 0, "both pressed together in a menu, held into play: no chord");
    GtChordStep(&c, 0, 0, dt, 1);
    Check(GtChordStep(&c, 1, 1, dt, 1) == GTC_ACT_LAST, "after the menu, both let go and pressed again in play: a chord");
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    GtChordStep(&c, 0, 1, dt, 0);  // Action down in a menu, let go there
    GtChordStep(&c, 0, 0, dt, 0);
    GtChordStep(&c, 1, 0, dt, 1);  // in play: Run, then Action 1 tick later
    Check(GtChordStep(&c, 1, 1, dt, 1) == GTC_ACT_LAST, "a menu press let go there does not block a fresh chord in play");
    // release both and press again: a new chord
    GtChordStep(&c, 0, 0, dt, 1);
    Check(GtChordStep(&c, 1, 1, dt, 1) == GTC_ACT_LAST, "both let go and pressed again: the next chord");
    // a stall between the presses does not count as time
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    GtChordStep(&c, 1, 0, dt, 1);
    GtChordStep(&c, 1, 0, 2.0f, 1);
    Check(GtChordStep(&c, 1, 1, dt, 1) == GTC_ACT_LAST, "a 2 s stall between the presses: no time passes (a chord)");
    GtChordReset(&c);
    GtChordStep(&c, 0, 0, dt, 1);
    GtChordStep(&c, 1, 0, -1.0f, 1);
    Check(GtChordStep(&c, 1, 1, dt, 1) == GTC_ACT_LAST, "a negative tick: no time passes");
    // the masks
    Check(GtMasksOk(0x2000, 0x1000) && !GtMasksOk(0x3000, 0x1000) && !GtMasksOk(0, 0x1000) && !GtMasksOk(0x10, 0),
          "the bindings: two different buttons only");
}

// ---- v3.44 GT344: Richard's fade (ScratchUpdateAlpha: GhostSetAlpha(1 - alpha))
static float RichardSee(float fadeIn, float fadeOut)  // the matched code's formula with rate 1
{
    float alpha = fadeIn > 0.0f ? fadeIn * 1.0f * 2.0f : 1.0f;
    if (fadeOut > 0.0f) alpha *= 1.0f - fadeOut * 2.0f;
    return 1.0f - alpha;
}
static void FadeRate(int hz)
{
    GtFade f;
    const float dt = 1.0f / (float)hz;
    float clock = 0.0f, warpAt = -1.0f, endAt = -1.0f, worst = 0.0f, prev = 0.0f;
    int k, warps = 0, mono = 1;
    char what[200];
    GtFadeReset(&f);
    Check(GtFadeSee(&f) == 0.0f, "idle: drawn as stock");
    GtFadeStart(&f);
    for (k = 0; k < hz * 2; ++k) {
        float see, rs;
        clock += dt;
        if (GtFadeStep(&f, dt)) ++warps, warpAt = clock;
        see = GtFadeSee(&f);
        rs = f.phase == GTF_OUT ? RichardSee(f.fi, 0.0f) : f.phase == GTF_IN ? RichardSee(0.0f, f.fo) : 0.0f;
        if (f.phase != GTF_IDLE && fabsf(see - rs) > worst) worst = fabsf(see - rs);
        if (warps == 0) mono &= see >= prev - 1e-6f;  // the fade-out only rises
        else if (f.phase == GTF_IN) mono &= see <= prev + 1e-6f || clock - warpAt < dt * 0.5f;
        prev = see;
        if (f.phase == GTF_IDLE && endAt < 0.0f) endAt = clock;
    }
    sprintf_s(what, sizeof what, "%d Hz: one warp at 0.5 s (%.3f), back to stock at 1.0 s (%.3f), the transparency = Richard's "
              "(worst %.2g), out rising, in falling", hz, warpAt, endAt, worst);
    Check(warps == 1 && warpAt >= GT_FADE_S - 1e-4f && warpAt <= GT_FADE_S + dt + 1e-4f && endAt >= 2.0f * GT_FADE_S - 1e-3f &&
          endAt <= 2.0f * GT_FADE_S + 2.0f * dt + 1e-3f && worst < 1e-5f && mono, what);
}
static void FadeRules(void)
{
    GtFade f;
    int k, w = 0;
    GtFadeStart(&f);
    Check(Near(GtFadeSee(&f), 0.0f, 1e-6f), "the fade-out starts fully drawn");
    for (k = 0; k < 15; ++k) w += GtFadeStep(&f, 1.0f / 60.0f);  // 0.25 s
    Check(!w && Near(GtFadeSee(&f), 0.5f, 1e-3f), "0.25 s in: half see-through");
    GtFadeBack(&f);
    Check(f.phase == GTF_IN && Near(GtFadeSee(&f), 0.5f, 1e-3f), "called off at 0.25 s: she fades back in from half");
    for (k = 0; k < 14; ++k) w += GtFadeStep(&f, 1.0f / 60.0f);
    Check(!w && f.phase == GTF_IN, "no warp while fading back");
    for (k = 0; k < 3; ++k) w += GtFadeStep(&f, 1.0f / 60.0f);
    Check(!w && f.phase == GTF_IDLE && GtFadeSee(&f) == 0.0f, "back to stock after the same 0.25 s, no warp");
    GtFadeStart(&f);
    GtFadeBack(&f);
    Check(f.phase == GTF_IDLE && GtFadeSee(&f) == 0.0f, "called off at once: stock at once");
    GtFadeStart(&f);
    Check(!GtFadeStep(&f, 2.0f) && f.phase == GTF_OUT && f.fi == GT_FADE_S, "a 2 s stall: no time passes in the fade");
    // the ghost pre-draw's FIX (0x45C7A0: (1 - a) x 128 - 0.49, round to nearest)
    Check(GtFadeFix(0.0f) == 128 && GtFadeFix(1.0f) == 0 && GtFadeFix(0.5f) == 64 && GtFadeFix(-3.0f) == 128 && GtFadeFix(7.0f) == 0,
          "FIX: 128 stock, 0 gone, 64 at half, clamped");
    {
        int ok = 1;
        float a;
        for (a = 0.0f; a <= 1.0f; a += 0.01f) {
            const float x = (1.0f - a) * 128.0f - 0.49f;
            ok &= GtFadeFix(a) == (int)floorf(x + 0.5f) || (x < 0.0f && GtFadeFix(a) == 0);
        }
        Check(ok, "FIX over 0 .. 1 matches the ghost's rounding");
    }
    Check(GtFadeFix(sqrtf(-1.0f)) == 128, "a NaN transparency: drawn as stock");
}

// ---- v3.44 GT344: the warp ahead
static void Ahead(void)
{
    const float p[3] = { 100.0f, -20.0f, 50.0f };
    float c[3];
    int k, ok = 1;
    Check(GtFwdKey(0, 0.0f) == GT_FWD_DEF && GtFwdKey(1, -5.0f) == GT_FWD_DEF && GtFwdKey(1, sqrtf(-1.0f)) == GT_FWD_DEF,
          "ahead key: missing or bad = 800");
    Check(GtFwdKey(1, 0.0f) == 0.0f, "ahead key: 0 = off");
    Check(GtFwdKey(1, 50.0f) == GT_FWD_MIN && GtFwdKey(1, 9000.0f) == GT_FWD_MAX && GtFwdKey(1, 1200.0f) == 1200.0f,
          "ahead key: clamped 300 .. 2000");
    Check(GtFwdTries(800.0f) == 7 && GtFwdTries(300.0f) == 2 && GtFwdTries(2000.0f) == 19 && GtFwdTries(0.0f) == 0 &&
          GtFwdTries(850.0f) == 8 && GtFwdTries(200.0f) == 1, "tries: 800 -> 7 (800 .. 200), 300 -> 2, 2000 -> 19, "
          "850 -> 8 (850 .. 250, 200), 200 -> 1, off -> 0");
    for (k = 0; k < GtFwdTries(800.0f); ++k) {
        const float yaw = 0.9f;
        float dx, dz, d;
        GtFwdCand(p, yaw, 800.0f, k, c);
        dx = c[0] - p[0], dz = c[2] - p[2], d = sqrtf(dx * dx + dz * dz);
        ok &= Near(d, 800.0f - 100.0f * (float)k, 1e-2f) && Near(dx / d, sinf(yaw), 1e-5f) && Near(dz / d, cosf(yaw), 1e-5f) && c[1] == p[1];
    }
    Check(ok, "the tries: 800, 700 .. 200 u along her facing (sin yaw, cos yaw), at her floor");
    GtFwdCand(p, 0.0f, 800.0f, GtFwdTries(800.0f) - 1, c);
    Check(Near(c[2] - p[2], GT_FWD_SHORT, 1e-3f), "the last try is the shortest (200 u)");
    {   // v3.44 GT344-FWD-1: a distance off the 100 u grid still ends at exactly 200 u
        static const float kDist[] = { 850.0f, 333.0f, 1999.0f, 300.0f, 2000.0f, 1234.5f };
        int j, ok2 = 1;
        for (j = 0; j < (int)(sizeof kDist / sizeof kDist[0]); ++j) {
            const int n = GtFwdTries(kDist[j]);
            float prev = 1e9f;
            for (k = 0; k < n; ++k) {
                const float d = GtFwdDist(kDist[j], k);
                ok2 &= d < prev && d >= GT_FWD_SHORT && (k == 0 ? d == kDist[j] : prev - d <= GT_FWD_STEP + 1e-3f);
                prev = d;
            }
            ok2 &= n >= 1 && n <= GT_FWD_TRIES_MAX && GtFwdDist(kDist[j], n - 1) == GT_FWD_SHORT;
        }
        GtFwdCand(p, 0.0f, 850.0f, GtFwdTries(850.0f) - 1, c);
        ok2 &= Near(c[2] - p[2], GT_FWD_SHORT, 1e-3f) && Near(GtFwdDist(850.0f, 6), 250.0f, 1e-3f);
        Check(ok2, "off-grid distances (850, 333, 1999, 1234.5): full first, falling at most 100 u a try, the last exactly 200 u");
    }
    Check(GtHeldOk(100.0f, 0.0f) && !GtHeldOk(130.0f, 0.0f) && GtHeldOk(190.0f, 0.1f) && !GtHeldOk(500.0f, 0.1f) &&
          !GtHeldOk(sqrtf(-1.0f), 0.05f), "held: 120 u + 800 u/s of her stride; a snap back of 500 u is caught");
}

static void Pick(void)
{
    GtCand c[8];
    int o[GT_PICK_MAX], n;
    memset(c, 0, sizeof c);
    c[0].d = 900.0f, c[0].ok = 1;
    c[1].d = 300.0f, c[1].ok = 0;               // down
    c[2].d = 400.0f, c[2].ok = 1;
    c[3].d = 2600.0f, c[3].ok = 1;              // out of range
    c[4].d = 200.0f, c[4].ok = 1, c[4].dy = -700.0f;  // another floor
    c[5].d = 1200.0f, c[5].ok = 1;
    c[6].d = 100.0f, c[6].ok = 1, c[6].dy = 300.0f;
    c[7].d = 1100.0f, c[7].ok = 1;
    n = GtPick(c, 8, GT_RANGE_DEF, o, GT_PICK_MAX);
    Check(n == 3 && o[0] == 6 && o[1] == 2 && o[2] == 0, "the nearest three standing enemies in range and height, nearest first");
    n = GtPick(c, 8, 350.0f, o, GT_PICK_MAX);
    Check(n == 1 && o[0] == 6, "a shorter range");
    n = GtPick(c, 2, GT_RANGE_DEF, o, GT_PICK_MAX);
    Check(n == 1 && o[0] == 0, "only the standing one");
    c[0].ok = c[2].ok = c[5].ok = c[6].ok = c[7].ok = 0;
    Check(GtPick(c, 8, GT_RANGE_DEF, o, GT_PICK_MAX) == 0, "none standing in range: none (the warp ahead)");
    {   // GT343-004: a full list of GT_ACTOR_MAX, the nearest ones at its end (Enemy Copies come last): none dropped
        static GtCand m[GT_ACTOR_MAX];
        int k;
        for (k = 0; k < GT_ACTOR_MAX; ++k) m[k].d = 2400.0f - 10.0f * (float)k, m[k].dy = 0.0f, m[k].ok = 1;
        m[GT_ACTOR_MAX - 1].d = 5.0f;
        m[40].d = 6.0f;
        n = GtPick(m, GT_ACTOR_MAX, GT_RANGE_DEF, o, GT_PICK_MAX);
        Check(n == 3 && o[0] == GT_ACTOR_MAX - 1 && o[1] == 40 && o[2] == GT_ACTOR_MAX - 2,
              "128 actors: the nearest at index 127 and 40 (past v3.43's first cap of 32) are picked first");
        Check(GT_ACTOR_MAX >= 64, "the list bound is at least the records table (64)");
    }
    {   // many in reverse order: still the nearest three
        GtCand m[20];
        int k;
        for (k = 0; k < 20; ++k) m[k].d = 2000.0f - 90.0f * (float)k, m[k].dy = 0.0f, m[k].ok = 1;
        n = GtPick(m, 20, GT_RANGE_DEF, o, GT_PICK_MAX);
        Check(n == 3 && o[0] == 19 && o[1] == 18 && o[2] == 17, "20 candidates: the nearest three");
    }
}

static void QuatYaw(float yaw, float q[4]) { q[0] = 0.0f, q[1] = sinf(0.5f * yaw), q[2] = 0.0f, q[3] = cosf(0.5f * yaw); }
static void Spots(void)
{
    const float pp[3] = { 0.0f, 0.0f, 0.0f }, e[3] = { 0.0f, 0.0f, 1000.0f };
    float q[4], b[2], c[3], yawS;
    int k, ok = 1, fromFace;
    char what[160];
    for (k = -6; k <= 6; ++k) {  // its facing yaw: back = -(sin, cos)
        const float yaw = 0.5f * (float)k;
        QuatYaw(yaw, q);
        fromFace = GtBackDir(q, e, pp, b);
        ok &= fromFace && Near(b[0], -sinf(yaw), 1e-5f) && Near(b[1], -cosf(yaw), 1e-5f);
    }
    Check(ok, "the back direction from the model's yaw quaternion (13 yaws)");
    q[0] = 0.70710678f, q[1] = 0.0f, q[2] = 0.0f, q[3] = 0.70710678f;  // tipped 90 degrees about x: forward straight down
    fromFace = GtBackDir(q, e, pp, b);
    Check(!fromFace && Near(b[0], 0.0f, 1e-5f) && Near(b[1], 1.0f, 1e-5f), "a tipped-over model: the far side from her");
    q[1] = q[2] = 0.0f, q[3] = 1.0f;
    q[0] = sqrtf(-1.0f);
    fromFace = GtBackDir(q, e, pp, b);
    Check(!fromFace && Near(b[1], 1.0f, 1e-5f), "a NaN rotation: the far side from her");
    {   // on top of her: a fixed direction
        const float same[3] = { 5.0f, 0.0f, 5.0f };
        q[0] = sqrtf(-1.0f);
        fromFace = GtBackDir(q, same, same, b);
        Check(!fromFace && b[0] == 0.0f && b[1] == 1.0f, "no facing and no offset: +z");
    }
    // the rings
    QuatYaw(0.0f, q);  // it faces +z: its back is -z
    GtBackDir(q, e, pp, b);
    GtSpotCand(e, b, 40.0f, 0, c);
    Check(Near(c[0], 0.0f, 1e-3f) && Near(c[2], 1000.0f - 110.0f, 1e-3f) && c[1] == e[1], "spot 0: straight behind at its radius + 70");
    for (k = 0, ok = 1; k < GT_SPOT_N; ++k) {
        const int ring = k / GT_ANGS;
        float dx, dz, r, cosA;
        GtSpotCand(e, b, 40.0f, k, c);
        dx = c[0] - e[0], dz = c[2] - e[2];
        r = sqrtf(dx * dx + dz * dz);
        cosA = (dx * b[0] + dz * b[1]) / r;
        ok &= Near(r, 40.0f + GT_RING0 + GT_RING_STEP * (float)ring, 1e-2f) && cosA > -1e-4f;  // never in front of it
        if (k > 0) {
            float p[3];
            GtSpotCand(e, b, 40.0f, k - 1, p);
            ok &= fabsf(p[0] - c[0]) + fabsf(p[2] - c[2]) > 1.0f;
        }
    }
    Check(ok, "21 spots: on their rings, none in front of it, all distinct");
    // facing it from the spot
    GtSpotCand(e, b, 40.0f, 3, c);
    yawS = GtFaceYaw(c, e);
    {
        const float dx = e[0] - c[0], dz = e[2] - c[2], l = sqrtf(dx * dx + dz * dz);
        sprintf_s(what, sizeof what, "her yaw on the spot faces the enemy (%.3f rad)", yawS);
        Check(Near(sinf(yawS), dx / l, 1e-4f) && Near(cosf(yawS), dz / l, 1e-4f), what);
    }
    {   // the others
        const float others[2][3] = { { 0.0f, 0.0f, 880.0f }, { 500.0f, 0.0f, 0.0f } };
        float s0[3];
        GtSpotCand(e, b, 40.0f, 0, s0);
        Check(!GtClearOf(s0, others, 2), "a spot 10 u from another enemy is refused");
        Check(GtClearOf(s0, others + 1, 1), "far from the others: clear");
        Check(GtClearOf(s0, others, 0), "no others: clear");
    }
}

// GT343-001: the same room by its door lines and its box
static void Rooms(void)
{
    const GtExit door = { -50.0f, 0.0f, 50.0f, 0.0f, 0.0f, 0.0f, -100.0f };  // the room is on the -z side
    const float enemy[3] = { 0.0f, 0.0f, -300.0f };
    float s[3] = { 0.0f, 0.0f, -600.0f };
    GtExit x[2];
    x[0] = door;
    Check(GtExitTest(x, 1, enemy, s) == GTX_OK, "a spot deep in the room: ok");
    s[2] = -80.0f;
    Check(GtExitTest(x, 1, enemy, s) == GTX_NEAR, "80 u from the door line: refused (at a door)");
    s[2] = 200.0f;
    Check(GtExitTest(x, 1, enemy, s) == GTX_BEYOND, "200 u beyond the door line, in its span: the next room");
    s[2] = 200.0f, s[0] = 160.0f;
    Check(GtExitTest(x, 1, NULL, s) == GTX_BEYOND, "beyond it at the span's widened edge: the next room");
    s[0] = 0.0f, s[2] = 600.0f;
    Check(GtExitTest(x, 1, enemy, s) == GTX_CROSS, "600 u beyond, reached through the doorway: refused");
    Check(GtExitTest(x, 1, NULL, s) == GTX_OK, "the same point without the enemy's line: no rule says no");
    s[0] = 300.0f, s[2] = 200.0f;
    Check(GtExitTest(x, 1, enemy, s) == GTX_OK, "past the door's span (a wall's other side, its own tests decide): ok");
    s[0] = 0.0f, s[2] = 200.0f, s[1] = -900.0f;
    Check(GtExitTest(x, 1, enemy, s) == GTX_OK, "a door line on another level (900 u apart): not this spot's");
    s[1] = 0.0f;
    x[1] = door, x[1].px = 0.0f, x[1].pz = 0.0f;  // its room point on the line: no side known
    Check(GtExitTest(x + 1, 1, NULL, s) == GTX_OK, "a door with its room point on the line: no beyond test");
    x[1] = door, x[1].x1 = x[1].x0, x[1].z1 = x[1].z0;  // a point door
    s[0] = -50.0f, s[2] = 60.0f;
    Check(GtExitTest(x + 1, 1, enemy, s) == GTX_NEAR, "a point door: only the near test (60 u: near)");
    s[2] = 300.0f;
    Check(GtExitTest(x + 1, 1, enemy, s) == GTX_OK, "a point door 300 u away: ok");
    s[0] = 0.0f, s[2] = 200.0f;
    Check(GtExitTest(x, 0, enemy, s) == GTX_OK, "a room without exits: ok");
    {   // v3.44 GT344: the warp ahead from her own place toward a doorway: the same rules, her place as the anchor
        const float her[3] = { 0.0f, 0.0f, -500.0f };
        float a[3];
        int k, firstOk = -1;
        for (k = 0; k < GtFwdTries(800.0f); ++k) {  // she faces +z (yaw 0), the door line 500 u ahead
            GtFwdCand(her, 0.0f, 800.0f, k, a);
            if (firstOk < 0 && GtExitTest(x, 1, her, a) == GTX_OK) firstOk = k;
        }
        Check(firstOk == 5, "ahead toward a doorway 500 u off: 800 / 700 u beyond it, 600 / 500 / 400 u at its line, "
                            "the first try in the room is 300 u");
    }
    // GT343-001c: the edges complete and valid, else no warp (fail closed)
    Check(GtBoundsVerdict(1, 4, 4, 1, 1) == GTB_OK, "4 exits all read, the box there, she inside: checked");
    Check(GtBoundsVerdict(1, 0, 0, 1, 1) == GTB_OK, "a room without exits, with its box and she inside: checked");
    Check(GtBoundsVerdict(1, 0, 0, 0, 0) == GTB_NOBOX, "no exits and no box: refused");
    Check(GtBoundsVerdict(1, GT_EXIT_MAX + 1, GT_EXIT_MAX, 1, 1) == GTB_TRUNC, "more exits than the list holds: refused");
    Check(GtBoundsVerdict(1, -1, 0, 1, 1) == GTB_TRUNC, "a negative exit count: refused");
    Check(GtBoundsVerdict(1, 5, 4, 1, 1) == GTB_ENTRY, "one exit entry null or NaN: refused");
    Check(GtBoundsVerdict(1, 3, 3, 0, 0) == GTB_NOBOX, "exits read but no collision box: refused");
    Check(GtBoundsVerdict(1, 3, 3, 1, 0) == GTB_OUTSIDE, "she stands outside the box: refused");
    Check(GtBoundsVerdict(0, 0, 0, 1, 1) == GTB_ROOM, "a room number outside the tables: refused");
    {
        int k2, named = 1;
        for (k2 = 1; k2 < GTB_N; ++k2) named &= kGtBoundsWhy[k2] && kGtBoundsWhy[k2][0];
        Check(named, "every refusal has its reason text");
    }
    {
        const float lo[3] = { -1000.0f, -500.0f, -1000.0f }, hi[3] = { 1000.0f, 500.0f, 1000.0f };
        const float in[3] = { 999.0f, 0.0f, -999.0f }, out[3] = { 1001.0f, 0.0f, 0.0f }, out2[3] = { 0.0f, 0.0f, -1200.0f };
        Check(GtInBox(lo, hi, in) && !GtInBox(lo, hi, out) && !GtInBox(lo, hi, out2), "the room's box (xz)");
    }
}

static void Keys(void)
{
    Check(GtRangeKey(0, 0.0f) == GT_RANGE_DEF && GtRangeKey(1, -5.0f) == GT_RANGE_DEF, "range: missing or bad = 2500");
    Check(GtRangeKey(1, 100.0f) == GT_RANGE_MIN && GtRangeKey(1, 9000.0f) == GT_RANGE_MAX && GtRangeKey(1, 1800.0f) == 1800.0f,
          "range: clamped 500 .. 6000");
    Check(GtCoolKey(0, 0.0f) == GT_COOL_DEF && GtCoolKey(1, 0.1f) == GT_COOL_MIN && GtCoolKey(1, 99.0f) == GT_COOL_MAX &&
          GtCoolKey(1, 5.0f) == 5.0f, "cooldown: the default, clamped 0.5 .. 30");
    // v3.49 GT349: no cooldown unless the ini asks for one; an old explicit value still reads
    Check(GT_COOL_DEF == 0.0f && GtCoolKey(0, 0.0f) == 0.0f && GtCoolKey(1, 0.0f) == 0.0f && GtCoolKey(1, -2.0f) == 0.0f &&
          GtCoolKey(1, 3.0f) == 3.0f, "v3.49: cooldown 0 by default (missing, 0 or negative); GhostTeleportCooldown=3 still 3");
}

// ---------------------------------------------------------------- v3.49 GT349: hold Run + Action = keep warping
// A simulated session at hz: a chord, then both held for `hold` seconds; every warp is Richard's fade (0.5 s out, the
// warp, 0.5 s in); a refusal waits GT_FAIL_COOL. Returns the warps made; *gapMin = the shortest time from one warp's
// fade-in end to the next warp's start.
static int HoldRun(int hz, float hold, float cool, int refuseEvery, float *gapMin, int *refusals)
{
    const float dt = 1.0f / (float)hz;
    GtHold h;
    GtFade f;
    float t, coolLeft = 0.0f, idleAt = -1.0f;
    int warps = 0, tries = 0;
    GtHoldReset(&h);
    GtFadeReset(&f);
    *gapMin = 1e9f, *refusals = 0;
    GtHoldStart(&h);  // the chord's tick: its own warp starts at once
    GtFadeStart(&f);
    ++tries;
    for (t = dt; t < hold + 3.0f; t += dt) {
        const int held = t < hold;
        int r;
        coolLeft = coolLeft > dt ? coolLeft - dt : 0.0f;
        r = GtHoldStep(&h, held, held, 1, f.phase == GTF_IDLE, coolLeft, dt);
        if (r == GTH_GO) {
            ++tries;
            if (refuseEvery && tries % refuseEvery == 0) {
                GtHoldRefused(&h);
                ++*refusals;
            } else {
                if (idleAt >= 0.0f && t - idleAt < *gapMin) *gapMin = t - idleAt;
                GtFadeStart(&f);
            }
        }
        if (f.phase != GTF_IDLE) {
            if (GtFadeStep(&f, dt)) ++warps, coolLeft = cool;
            if (f.phase == GTF_IDLE) idleAt = t;
        }
    }
    return warps;
}
static void Hold(void)
{
    static const int kHz[4] = { 30, 60, 120, 144 };
    int i, n, refused;
    float gap;
    for (i = 0; i < 4; ++i) {
        char what[160];
        n = HoldRun(kHz[i], 5.0f, 0.0f, 0, &gap, &refused);
        sprintf_s(what, sizeof what, "hold %d Hz: 5 s held = 5 warps (a warp a second: 0.5 s out + 0.5 s in), the next one "
                  "a tick after the fade-in (%d warps, gap %.3f s)", kHz[i], n, gap);
        Check(n == 5 && gap <= 1.5f / (float)kHz[i], what);
        n = HoldRun(kHz[i], 0.2f, 0.0f, 0, &gap, &refused);
        sprintf_s(what, sizeof what, "hold %d Hz: one press (let go after 0.2 s) = one warp (%d)", kHz[i], n);
        Check(n == 1, what);
        n = HoldRun(kHz[i], 5.0f, 2.0f, 0, &gap, &refused);
        sprintf_s(what, sizeof what, "hold %d Hz: an ini cooldown of 2 s (from the warp) still spaces the held warps: 1.5 s "
                  "after the fade-in (%d warps, gap %.2f s)", kHz[i], n, gap);
        Check(n == 2 && gap >= 1.49f && gap <= 1.5f + 1.5f / (float)kHz[i], what);
        n = HoldRun(kHz[i], 5.0f, 0.0f, 2, &gap, &refused);
        sprintf_s(what, sizeof what, "hold %d Hz: refused tries wait %.1f s, the hold goes on (%d warps, %d refused)", kHz[i],
                  GT_FAIL_COOL, n, refused);
        Check(n >= 3 && n <= 4 && refused >= 2, what);
    }
    {   // the hold ends with either button or outside gameplay, and only then
        GtHold h;
        GtHoldReset(&h);
        Check(GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.016f) == GTH_NONE, "hold: no hold without a chord");
        GtHoldStart(&h);
        Check(GtHoldStep(&h, 1, 1, 1, 0, 0.0f, 0.016f) == GTH_NONE, "hold: nothing while a fade runs");
        Check(GtHoldStep(&h, 1, 1, 1, 1, 0.3f, 0.016f) == GTH_NONE, "hold: nothing while a cooldown runs");
        Check(GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.016f) == GTH_GO && h.on, "hold: idle and no cooldown = the next warp");
        Check(GtHoldStep(&h, 1, 0, 1, 1, 0.0f, 0.016f) == GTH_END && !h.on, "hold: Action let go = the end");
        GtHoldStart(&h);
        Check(GtHoldStep(&h, 0, 1, 1, 1, 0.0f, 0.016f) == GTH_END, "hold: Run let go = the end");
        GtHoldStart(&h);
        Check(GtHoldStep(&h, 1, 1, 0, 1, 0.0f, 0.016f) == GTH_END, "hold: a menu, a scene or a load = the end");
        Check(GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.016f) == GTH_NONE, "hold: ended stays ended (a new press needed)");
        GtHoldStart(&h);
        GtHoldRefused(&h);
        Check(h.refused == 1 && h.wait == GT_FAIL_COOL && GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.2f) == GTH_NONE &&
              GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.2f) == GTH_NONE && GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.2f) == GTH_NONE &&
              GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 0.016f) == GTH_GO, "hold: a refusal waits 0.5 s");
        GtHoldRefused(&h);
        Check(GtHoldStep(&h, 1, 1, 1, 1, 0.0f, 1.0f) == GTH_NONE && h.wait == GT_FAIL_COOL,
              "hold: a stalled tick (over 0.25 s) does not use up the wait");
        GtHoldReset(&h);
        GtHoldRefused(&h);
        Check(h.refused == 0, "hold: a refusal without a hold counts nothing");
    }
}

// ---------------------------------------------------------------- v3.49 GT349: walk-in event zones
static void Zones(void)
{
    // Subway room 2:2, event 00A (record 0x62A6D0, "Creatures I"): box at (9.5, 0, -2147), 1050 x 100 x 447, heading 0
    const GtZone ev00A = { 9.5f, 0.0f, -2147.0f, 2, 0.0f, 1050.0f, 100.0f, 447.0f };
    const float w15a[3] = { -11.0f, 0.0f, -2867.0f }, w15b[3] = { -131.0f, 0.0f, -1805.0f };  // v3.48's warp 15
    const float w14a[3] = { -31.0f, 0.0f, -6024.0f }, w14b[3] = { 46.0f, 0.0f, -5228.0f };    // warp 14
    const float near[3] = { -11.0f, 0.0f, -2500.0f }, inBox[3] = { 0.0f, 0.0f, -2100.0f }, past[3] = { 0.0f, 0.0f, -1600.0f };
    const float edge[3] = { -11.0f, 0.0f, -2385.0f }, upstairs[3] = { -131.0f, -900.0f, -1805.0f }, upFrom[3] = { -11.0f, -900.0f, -2867.0f };
    const float nanP[3] = { NAN, 0.0f, 0.0f };
    GtZone z;
    Check(GtZoneHit(&ev00A, w15a, w15b), "zones: v3.48's crash warp 15 (z -2867 -> -1805) jumps over event 00A's box: refused");
    Check(!GtZoneHit(&ev00A, w14a, w14b), "zones: warp 14 (south of the box) still goes");
    Check(!GtZoneHit(&ev00A, w15a, near), "zones: a spot short of the box still goes");
    Check(GtZoneHit(&ev00A, w15a, inBox), "zones: a spot inside the box is refused (the event would fire under her)");
    Check(GtZoneHit(&ev00A, w15a, edge), "zones: a spot just outside the box's edge (within the pad) is refused");
    // v3.49 GT349-001 (Codex inspection 1): inside a listed zone (its event not running yet) = no warp out of it
    Check(GtZoneHit(&ev00A, inBox, past) && GtZoneInside(&ev00A, inBox) && GtZonesInside(&ev00A, 1, inBox) == 0,
          "GT349-001: standing inside a zone whose event has not started: no warp out (it fires first)");
    {   // she runs into the box during the 0.5 s fade-out (from the press at w15a's z -2500 to z -2100), then the warp
        const float press[3] = { -11.0f, 0.0f, -2500.0f }, atWarp[3] = { -11.0f, 0.0f, -2100.0f };
        const float ahead[3] = { -11.0f, 0.0f, -1300.0f }, back[3] = { -11.0f, 0.0f, -2900.0f };
        Check(!GtZoneHit(&ev00A, press, near) && GtZonesInside(&ev00A, 1, press) < 0,
              "GT349-001: at the press (short of the box) a spot short of it still goes");
        Check(GtZonesInside(&ev00A, 1, atWarp) == 0 && GtZoneHit(&ev00A, atWarp, ahead) && GtZoneHit(&ev00A, atWarp, back),
              "GT349-001: entered during the fade-out: the warp at its end is refused, ahead or back");
    }
    // the list's filter (EventObjectsTick's test): a running event's zone is not listed, so standing in it she may warp
    Check(GtZoneArmed(0, 0, 1, 9, 2, 0) == 1 && GtZoneArmed(0, 1, 1, 9, 2, 0) == 0 && GtZoneArmed(0, 0, 0, 9, 2, 0) == 0 &&
          GtZoneArmed(1, 0, 1, 9, 2, 0) == 0 && GtZoneArmed(0, 0, 1, 4, 2, 0) == 0 && GtZoneArmed(0, 0, 1, 8, 2, 0) == 0 &&
          GtZoneArmed(0, 0, 1, 7, 1, 0) == 1 && GtZoneArmed(0, 0, 1, 9, 5, 0) == -1 && GtZoneArmed(0, 0, 1, 9, 2, 4) == -1 &&
          GtZoneArmed(0, 0, 1, 9, 3, 0x10) == -1, "GT349-001: listed = enabled, not running, no use-item, type 7 / 9, shape 1..3, no press / inverted");
    {
        GtZone listed[1];
        int n = 0;
        if (GtZoneArmed(0, 1, 1, 9, 2, 0) > 0) listed[n++] = ev00A;  // event 00A running (its scene started)
        Check(n == 0 && GtZonesInside(listed, n, inBox) < 0 && GtZonesHit(listed, n, inBox, past) < 0,
              "GT349-001: inside a zone whose event runs: not listed, the warp goes");
    }
    Check(!GtZoneHit(&ev00A, edge, w15a), "zones: from its edge, away from it: allowed");
    Check(GtZoneHit(&ev00A, edge, past), "zones: from its edge, across it: refused");
    Check(!GtZoneHit(&ev00A, upFrom, upstairs), "zones: the same path a floor up (900 u) does not meet its height");
    Check(GtZoneHit(&ev00A, w15a, nanP), "zones: a spot that is not a number: refused (fail closed)");
    z = ev00A, z.sx = NAN;
    Check(GtZoneHit(&z, w14a, w14b), "zones: a zone value that is not a number: refused (fail closed)");
    z = ev00A, z.shape = 5;
    Check(!GtZoneHit(&z, w15a, w15b), "zones: shape 5 (always) / others are not walk-in boxes: not tested here");
    z = ev00A, z.heading = 1.5707963f;  // turned 90 degrees: 447 wide in x, 1050 deep in z
    {
        const float a[3] = { -300.0f, 0.0f, -2867.0f }, b[3] = { -300.0f, 0.0f, -1805.0f };
        Check(!GtZoneHit(&z, a, b), "zones: a box turned 90 degrees swaps its sizes (x -214 .. 233 now: x -300 passes)");
        Check(GtZoneHit(&ev00A, a, b), "zones: the same path meets the unturned box");
    }
    z = ev00A, z.heading = 0.7853982f;  // turned 45 degrees: the enclosing circle (r = 571 + pad)
    {
        const float a[3] = { 9.5f + 580.0f, 0.0f, -3000.0f }, b[3] = { 9.5f + 580.0f, 0.0f, -1000.0f };
        const float c[3] = { 9.5f + 600.0f, 0.0f, -3000.0f }, d[3] = { 9.5f + 600.0f, 0.0f, -1000.0f };
        Check(GtZoneHit(&z, a, b) && !GtZoneHit(&z, c, d), "zones: a box turned 45 degrees = its enclosing circle (more refused)");
    }
    {   // a cylinder (radius 200, height 300) and a sphere (radius 150)
        const GtZone cyl = { 1000.0f, 0.0f, 0.0f, 1, 0.0f, 200.0f, 300.0f, 0.0f }, sph = { 0.0f, -100.0f, 1000.0f, 3, 0.0f, 150.0f, 0.0f, 0.0f };
        const float a[3] = { 0.0f, 0.0f, 0.0f }, b[3] = { 2000.0f, 0.0f, 0.0f }, b2[3] = { 2000.0f, 0.0f, 300.0f };
        const float s1[3] = { 0.0f, 0.0f, 2000.0f }, s2[3] = { 300.0f, 0.0f, 2000.0f };
        Check(GtZoneHit(&cyl, a, b) && GtZoneHit(&cyl, a, b2), "zones: a cylinder on the path (through it, or 150 u off its "
              "centre): refused");
        Check(!GtZoneHit(&cyl, b2, (const float[3]){ 2000.0f, 0.0f, 1000.0f }), "zones: a cylinder off the path: allowed");
        Check(GtZoneHit(&sph, a, s1) && !GtZoneHit(&sph, s2, (const float[3]){ 300.0f, 0.0f, 0.0f }), "zones: a sphere on / off the path");
    }
    {   // the list: the first zone hit, -1 for none
        GtZone zs[3];
        zs[0] = ev00A, zs[0].x = 5000.0f, zs[1] = ev00A, zs[2] = ev00A;
        Check(GtZonesHit(zs, 3, w15a, w15b) == 1 && GtZonesHit(zs, 1, w15a, w15b) == -1 && GtZonesHit(zs, 0, w15a, w15b) == -1,
              "zones: the first zone in the list that the path meets");
    }
    {   // the rectangle test itself: touching, parallel, a point
        Check(GtSegRect(-10.0f, 5.0f, 10.0f, 5.0f, 2.0f, 5.0f) && !GtSegRect(-10.0f, 5.1f, 10.0f, 5.1f, 2.0f, 5.0f) &&
              GtSegRect(0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f) && !GtSegRect(3.0f, 3.0f, 3.0f, 3.0f, 1.0f, 1.0f) &&
              !GtSegRect(-10.0f, 0.0f, -5.0f, 0.0f, 2.0f, 2.0f), "zones: the segment / rectangle test (edges, points, short of it)");
    }
}

int main(int argc, char **argv)
{
    static const int kHz[4] = { 30, 60, 120, 144 };
    int i;
    g_verbose = argc > 1 && !strcmp(argv[1], "-v");
    ChordRates();  // v3.44 GT344
    ChordRules();
    for (i = 0; i < 4; ++i) FadeRate(kHz[i]);
    FadeRules();
    Ahead();
    Pick();
    Spots();
    Rooms();  // GT343-001
    Keys();
    Hold();   // v3.49 GT349
    Zones();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "PASSED", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
