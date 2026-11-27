// v3.43 GT343 / v3.44 GT344: teleport_sim.h native test (staging/v343_feedback/w2_teleport/PLAN.md,
// staging/v344_feedback/w3_teleport/PLAN.md): Ghost Teleport's trigger (v3.44: Run + Action within 80 ms, either first,
// at 30 / 60 / 120 / 144 Hz; each press used once; a press held from before is none); Richard's fade (0.5 s out, the
// warp, 0.5 s in; called off = back in from where it was; the ghost pre-draw's FIX); the warp ahead (the key, the
// tries); the nearest-first pick (range, height, standing only, at most GT_PICK_MAX); the back direction from the
// model's quaternion (and its fallback); the spot rings; the facing; the clearance from other enemies; the room's
// edges; the hidden keys; the move-held check.
// v3.52 TP352 (staging/v352_feedback/w5_teleport/PLAN.md): Proof() replays warp 129 of the user's v3.51 log (the spot
// behind the enemy that put her in a Water Prison wall) on a box world: v3.51's rules took it, the landing proof refuses
// it; the warp ahead's spot at a wall; the proof's answers; the map's seeds inside collision; GtStuck; GtFreeCell (the
// nearest proven floor cell, against every cell of the map); FreeWatch(): the watch after a warp at 30 .. 240 Hz.
// v3.58 GT358 (staging/v358_feedback/w6_teleport/PLAN.md): FadeDraw(): the clean fade's plan (late on a pre-pass, in place
// by shader 4 / 5, every gate), her opacity (= the draw's fix / 128), the shadow / heat / soul rules, the hidden key, and a
// whole fade at 180 Hz without a jump.
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

// ---- v3.58 GT358: the clean fade's plan, her opacity for the heat body / the soul fire / the shadow, the hidden key
static void FadeDraw(void)
{
    static const float kSee[] = { -1.0f, 0.0f, 0.004f, 0.25f, 0.5f, 0.75f, 0.996f, 1.0f, 2.0f };
    GtFadeDraw d;
    int m, i, ok;
    // the key: missing / out of range = Clean; 0 / 1 / 2 kept
    Check(GtFadeDrawKey(0, 0) == GT_FD_CLEAN && GtFadeDrawKey(1, 0) == GT_FD_STOCK && GtFadeDrawKey(1, 1) == GT_FD_ONE &&
          GtFadeDrawKey(1, 2) == GT_FD_CLEAN && GtFadeDrawKey(1, 3) == GT_FD_CLEAN && GtFadeDrawKey(1, -1) == GT_FD_CLEAN,
          "GhostTeleportFadeDraw: 0 / 1 / 2 read, anything else Clean");
    Check(GtFadeDrawName(0)[0] == 'S' && GtFadeDrawName(1)[0] == 'O' && GtFadeDrawName(2)[0] == 'C', "the modes' names");
    // her opacity is the draw's own alpha: fix / 128
    ok = 1;
    for (i = 0; i < (int)(sizeof kSee / sizeof kSee[0]); ++i) ok &= GtFadeOp(kSee[i]) == (float)GtFadeFix(kSee[i]) / 128.0f;
    Check(ok && GtFadeOp(0.0f) == 1.0f && GtFadeOp(1.0f) == 0.0f && GtFadeOp(0.5f) == 0.5f && GtFadeOp(sqrtf(-1.0f)) == 1.0f,
          "opacity = the draw's fix / 128 (1 stock, 0 gone, NaN stock)");
    // not fading: stock (every mode, every gate)
    for (m = 0; m <= 2; ++m) {
        GtFadePlan(&d, m, 0.0f, 1, 1, 1);
        Check(d.path == GTD_NONE && d.ps == 8 && !d.prepass && d.fix == 128 && d.drawn, "not fading: the stock draw (any mode)");
        GtFadePlan(&d, m, sqrtf(-1.0f), 1, 1, 1);
        Check(d.path == GTD_NONE, "a NaN transparency: the stock draw");
    }
    // Clean with every gate: late, pre-pass, shader 5; a closed gate: in place with shader 5 (never shader 4)
    GtFadePlan(&d, GT_FD_CLEAN, 0.5f, 1, 1, 1);
    Check(d.path == GTD_LATE && d.ps == 5 && d.prepass && d.fix == 64 && d.drawn, "Clean: after the actor loop, pre-pass, shader 5");
    GtFadePlan(&d, GT_FD_CLEAN, 0.5f, 0, 1, 1);
    Check(d.path == GTD_INPLACE && d.ps == 5 && !d.prepass, "Clean without the hook: one pass in place, shader 5");
    GtFadePlan(&d, GT_FD_CLEAN, 0.5f, 1, 0, 1);
    Check(d.path == GTD_INPLACE && d.ps == 5 && !d.prepass, "Clean outside phase 0 (a scene starting this frame): in place");
    GtFadePlan(&d, GT_FD_CLEAN, 0.5f, 1, 1, 0);
    Check(d.path == GTD_INPLACE && d.ps == 5 && !d.prepass, "Clean while another redraw holds the override: in place");
    GtFadePlan(&d, GT_FD_ONE, 0.5f, 1, 1, 1);
    Check(d.path == GTD_INPLACE && d.ps == 5 && !d.prepass, "One pass: in place, shader 5");
    GtFadePlan(&d, GT_FD_STOCK, 0.5f, 1, 1, 1);
    Check(d.path == GTD_INPLACE && d.ps == 4 && !d.prepass && d.fix == 64, "Stock: v3.57's shader 4 in place");
    GtFadePlan(&d, GT_FD_CLEAN, 1.0f, 1, 1, 1);
    Check(d.path == GTD_LATE && d.fix == 0 && !d.drawn, "fully gone: nothing drawn (the pose still built)");
    // the shadow: Fade In's rule (drawn from half opacity), v3.57 always
    Check(GtShadowKeep(GT_FD_CLEAN, 0.0f) && GtShadowKeep(GT_FD_CLEAN, 0.5f) && !GtShadowKeep(GT_FD_CLEAN, 0.51f) &&
          !GtShadowKeep(GT_FD_ONE, 0.75f) && GtShadowKeep(GT_FD_STOCK, 1.0f), "her shadow from half opacity on (v3.57: always)");
    // Thermal's heat body: alpha x opacity, solid only when not fading; continuous at both ends
    Check(GtHeatAlpha(GT_FD_CLEAN, 1.0f, 0.0f) == 1.0f && GtHeatAlpha(GT_FD_CLEAN, 1.0f, 0.5f) == 0.5f &&
          GtHeatAlpha(GT_FD_CLEAN, 0.6f, 0.5f) == 0.3f && GtHeatAlpha(GT_FD_CLEAN, 1.0f, 1.0f) == 0.0f &&
          GtHeatAlpha(GT_FD_STOCK, 1.0f, 0.75f) == 1.0f, "heat alpha = its base x her opacity (v3.57: the base)");
    Check(GtHeatSolid(GT_FD_CLEAN, 1, 0.0f) && !GtHeatSolid(GT_FD_CLEAN, 1, 0.004f) && !GtHeatSolid(GT_FD_CLEAN, 0, 0.0f) &&
          GtHeatSolid(GT_FD_STOCK, 1, 0.5f), "the heat body stays solid only while she is not fading");
    {   // no pop: along a whole fade (0.5 s out, 0.5 s in at 180 Hz) her opacity never jumps more than a frame's share
        GtFade f;
        float last = 1.0f, maxJump = 0.0f, op;
        int k, warp = 0;
        GtFadeStart(&f);
        for (k = 0; k < 400 && (f.phase != GTF_IDLE || k == 0); ++k) {
            warp |= GtFadeStep(&f, 1.0f / 180.0f);
            op = GtFadeOp(GtFadeSee(&f));
            if (fabsf(op - last) > maxJump) maxJump = fabsf(op - last);
            last = op;
        }
        Check(warp && f.phase == GTF_IDLE && last == 1.0f && maxJump <= 2.0f / 90.0f + 1e-4f,
              "a whole fade at 180 Hz: opacity 1 -> 0 -> 1 in steps of at most two 90ths (no pop at either end)");
    }
    Check(GtSoulK(GT_FD_CLEAN, 0.0f) == 1.0f && GtSoulK(GT_FD_ONE, 0.5f) == 0.5f && GtSoulK(GT_FD_CLEAN, 1.0f) == 0.0f &&
          GtSoulK(GT_FD_STOCK, 0.5f) == 1.0f, "the soul fire x her opacity (v3.57: full)");
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
    c[3].d = 6000.0f, c[3].ok = 1;              // out of range  // v3.50 TP350: 6000 (the range is 5000 now)
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
    Check(GtRangeKey(0, 0.0f) == GT_RANGE_DEF && GtRangeKey(1, -5.0f) == GT_RANGE_DEF && GT_RANGE_DEF == 5000.0f,
          "range: missing or bad = 5000 (v3.50 TP350: was 2500)");
    Check(GtRangeKey(1, 100.0f) == GT_RANGE_MIN && GtRangeKey(1, 20000.0f) == GT_RANGE_MAX && GtRangeKey(1, 1800.0f) == 1800.0f &&
          GtRangeKey(1, 2500.0f) == 2500.0f && GtRangeKey(1, 9000.0f) == 9000.0f, "range: clamped 500 .. 12000 (an old 2500 still reads)");
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

// ================================================================ v3.50 TP350: through walls, the faced enemy, the reach
// A box world like the game's (y grows downwards): the ground's top at y 0 (solid below it) except pits (its top lower),
// solid boxes (walls, props, raised floors). The queries the flood and the ahead search ask, answered exactly.
typedef struct { float x0, x1, y0, y1, z0, z1; } TBox;  // solid where x0 <= x <= x1, y0 <= y <= y1 (y0 the top), z0 <= z <= z1
typedef struct { float x0, x1, z0, z1, top; } TPit;      // the ground's top is `top` inside it
typedef struct { TBox b[24]; int nb; TPit p[4]; int np; int unknown; } TWorld;
static float TGround(const TWorld *t, float x, float z)
{
    int i;
    for (i = 0; i < t->np; ++i)
        if (x >= t->p[i].x0 && x <= t->p[i].x1 && z >= t->p[i].z0 && z <= t->p[i].z1) return t->p[i].top;
    return 0.0f;
}
static int TSolidAt(const TWorld *t, float x, float y, float z)
{
    int i;
    if (y > TGround(t, x, z)) return 1;
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        if (x > b->x0 && x < b->x1 && y > b->y0 && y < b->y1 && z > b->z0 && z < b->z1) return 1;
    }
    return 0;
}
static int TSolid(void *c, float x, float y, float z) { return ((TWorld *)c)->unknown ? -1 : TSolidAt((TWorld *)c, x, y, z); }
static int TFloor(void *c, float x, float yTop, float yBot, float z, float *fy)
{
    const TWorld *t = (const TWorld *)c;
    float best = TGround(t, x, z);
    int i;
    if (t->unknown) return -1;
    if (TSolidAt(t, x, yTop, z)) return 0;  // (the game's tree2 line finds none from inside solid)
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        if (x > b->x0 && x < b->x1 && z > b->z0 && z < b->z1 && b->y0 > yTop && b->y0 < best) best = b->y0;
    }
    if (!(best > yTop && best <= yBot)) return 0;
    *fy = best;
    return 1;
}
static float TClamp(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static int TSphere(void *c, float x, float y, float z, float r)
{
    const TWorld *t = (const TWorld *)c;
    int i, n = 0;
    if (t->unknown) return -1;
    if (y + r > TGround(t, x, z)) ++n;  // (the ground under it; a pit's edge is not modelled for spheres)
    for (i = 0; i < t->nb; ++i) {
        const TBox *b = &t->b[i];
        const float dx = x - TClamp(x, b->x0, b->x1), dy = y - TClamp(y, b->y0, b->y1), dz = z - TClamp(z, b->z0, b->z1);
        n += dx * dx + dy * dy + dz * dz < r * r;
    }
    return n;
}
static int TLine(void *c, const float a[3], const float b[3])  // sampled every 2 u (exact enough for 40 u walls)
{
    const TWorld *t = (const TWorld *)c;
    const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2], l = sqrtf(dx * dx + dy * dy + dz * dz);
    const int n = (int)(l / 2.0f) + 1;
    int i;
    if (t->unknown) return -1;
    for (i = 0; i <= n; ++i) {
        const float u = (float)i / (float)n;
        if (TSolidAt(t, a[0] + dx * u, a[1] + dy * u, a[2] + dz * u)) return 1;
    }
    return 0;
}
static void TWall(TWorld *t, float x0, float x1, float z0, float z1, float h)  // a box from the ground up h
{
    TBox *b = &t->b[t->nb++];
    b->x0 = x0, b->x1 = x1, b->z0 = z0, b->z1 = z1, b->y0 = -h, b->y1 = 0.0f;
}
// teleport.inc's land test on this world (its order; the game's own floor ray and room-to-stand are the floor query here)
enum { TL_OK, TL_BOX, TL_DOOR, TL_ZONE, TL_WALL, TL_FLOOR, TL_LEDGE, TL_BODY, TL_ROUTE, TL_PENDING, TL_N };
typedef struct {
    GtWorld w;
    TWorld t;
    float her[3], lo[3], hi[3];
    GtExit x[4];
    int nx;
    GtZone z[4];
    int nz;
    GtMap *m;
    int mapCells;  // cells the map may expand before the search (default: all)
} TScene;
static int TOpen(void *c, const float p[3])
{
    TScene *s = (TScene *)c;
    return p[0] >= s->lo[0] && p[0] <= s->hi[0] && p[2] >= s->lo[2] && p[2] <= s->hi[2] && GtThruOpen(&s->w, s->her, p);
}
static int TLand(void *c, const float p[3], float out[3])
{
    TScene *s = (TScene *)c;
    float fy, hp[3];
    int r;
    if (!GtInBox(s->lo, s->hi, p)) return TL_BOX;
    if (GtExitTest(s->x, s->nx, s->her, p) != GTX_OK) return TL_DOOR;
    if (GtZonesHit(s->z, s->nz, s->her, p) >= 0) return TL_ZONE;
    if (TSolidAt(&s->t, p[0], s->her[1] - 60.0f, p[2])) return TL_WALL;
    if (!TFloor(&s->t, p[0], s->her[1] - 60.0f, s->her[1] + 6000.0f, p[2], &fy)) return TL_FLOOR;
    if (fabsf(fy - s->her[1]) > GT_FLOOR_TOL) return TL_LEDGE;
    if (!GtBodyFree(&s->w, p[0], fy, p[2])) return TL_BODY;
    hp[0] = p[0], hp[1] = fy, hp[2] = p[2];
    memcpy(out, hp, sizeof hp);
    r = GtMapHas(s->m, &s->w, hp);
    return r == GTM_HAS ? TL_OK : r == GTM_UNKNOWN ? TL_PENDING : TL_ROUTE;
}
static GtMap g_tMap;  // (~200 KB: static)
static void TSceneInit(TScene *s, float half)  // her at the origin facing +z; the room's box +-half; no door
{
    memset(s, 0, sizeof *s);
    s->w.ctx = &s->t, s->w.floorAt = TFloor, s->w.sphere = TSphere, s->w.line = TLine, s->w.solid = TSolid;
    s->lo[0] = s->lo[2] = -half, s->hi[0] = s->hi[2] = half, s->lo[1] = -2000.0f, s->hi[1] = 2000.0f;
    s->m = &g_tMap, s->mapCells = 1 << 30;
}
static void TDoor(TScene *s, float x, float z)  // a door line across x - 100 .. x + 100 at z, its room side toward the origin
{
    GtExit *e = &s->x[s->nx++];
    const float side = z > 0.0f ? -150.0f : 150.0f;
    e->x0 = x - 100.0f, e->x1 = x + 100.0f, e->z0 = e->z1 = z, e->y = 0.0f, e->px = x, e->pz = z + side;
}
static void TMap(TScene *s)  // teleport.inc's map: the room's box and doors; seeds: every door's point, her feet
{
    int k;
    GtMapBegin(s->m, s->lo, s->hi, s->x, s->nx);
    for (k = 0; k < s->nx; ++k) GtMapSeed(s->m, &s->w, s->x[k].px, s->x[k].y, s->x[k].pz, GT_MAP_SEED_DY);
    GtMapSeed(s->m, &s->w, s->her[0], s->her[1], s->her[2], GT_REACH_UP);
    GtMapRun(s->m, &s->w, s->mapCells);
}
static int TThru(TScene *s, float yaw, float dist, GtThru *t, int *cnt, int *nObst)
{
    GtThruFns f;
    f.ctx = s, f.open = TOpen, f.land = TLand, f.pending = TL_PENDING;
    memset(cnt, 0, sizeof(int) * TL_N);
    TMap(s);
    return GtThruSearch(&f, s->her, yaw, dist, t, cnt, TL_N, nObst);
}
static void Through(void)
{
    static TScene s;
    GtThru t;
    int cnt[TL_N], nObst, ok;
    // 1. a full-width wall 40 thick at z 300 .. 340; a door at the far end (z 1900): through it, the nearest spot where
    //    her body (70 u spheres) fits past the wall: 340 + 70 -> the sample at 440
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    TDoor(&s, 0.0f, 1900.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.ray == 0 && t.k == 11 && t.kObst == 8 && Near(t.at[2], 440.0f, 0.5f) && Near(t.at[1], 0.0f, 0.1f) &&
          Near(t.yaw, 0.0f, 1e-6f) && t.refused == 2 && cnt[TL_BODY] >= 1 && nObst >= 1,
          "through: a wall 40 thick at 300: the nearest spot past it where her body fits (440), straight ahead, her facing kept");
    Check(s.m->seeds == 2 && GtMapFinished(s.m) && s.m->cells > 1000, "through: the room's floor map: from its door and her feet, finished");
    // 1b. v3.50 TP350-001 (Codex inspection 1): the nearest answer of ALL rays wins. The same wall plus a block 80 wide
    //     (z 340 .. 700) right behind it on the straight ray: straight ahead lands only past the block (800); the +20 degree
    //     ray clears the block's side just past the wall (k 11, 440): that one is taken, not the straight ray's
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    TWall(&s.t, -40.0f, 40.0f, 340.0f, 700.0f, 1000.0f);
    TDoor(&s, 0.0f, 1900.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    printf("  (1b: ok %d ray %d k %d at %.0f %.0f)\n", ok, t.ray, t.k, t.at[0], t.at[2]);
    Check(ok == 1 && t.ray != 0 && t.k <= 12, "through: the nearest landing of all rays wins (a side ray at 440, not straight ahead at 800)");
    // 2. no obstacle within the warp distance: none (the v3.44 warp ahead, farthest first, applies)
    TSceneInit(&s, 2000.0f);
    TDoor(&s, 0.0f, 1900.0f);
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && nObst == 0, "through: open floor ahead: no through-wall spot, no obstacle");
    // 3. a closed space past the wall (a second wall at 700 .. 740): no spot there has a route; with 800 u nothing fits,
    //    with 1200 the spot past the second wall (740 + 70 -> 840: the door's side)
    TSceneInit(&s, 600.0f);
    TWall(&s.t, -700.0f, 700.0f, 300.0f, 340.0f, 1000.0f);  // (the walls run past the room's box: no way round at its edge)
    TWall(&s.t, -700.0f, 700.0f, 700.0f, 740.0f, 1000.0f);
    s.hi[2] = 2000.0f;
    TDoor(&s, 0.0f, 1900.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(!ok && cnt[TL_ROUTE] >= 5 && cnt[TL_PENDING] == 0, "through: a closed space past the wall (no door, not her floor): refused (no route), every ray");
    ok = TThru(&s, 0.0f, 1200.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.k == 21 && Near(t.at[2], 840.0f, 0.5f), "through: with 1200 u the spot past the second wall (840): the door's floor");
    // ... the map not finished yet: the nearest candidate is "not known yet" (2), nothing farther is taken before it
    s.mapCells = 10;
    ok = TThru(&s, 0.0f, 1200.0f, &t, cnt, &nObst);
    Check(ok == 2 && t.k == 11 && !GtMapFinished(s.m), "through: an unfinished map: the nearest candidate is pending (the warp decides)");
    // 4. a turnstile-like barrier 120 high (below her chest): an obstacle by the low line; past it at 400 (her lower
    //    sphere 60 u from it, the upper one clears its top); the route: the door past it
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 320.0f, 120.0f);
    TDoor(&s, 0.0f, 1900.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.kObst == 8 && t.k == 10 && Near(t.at[2], 400.0f, 0.5f), "through: a barrier 120 high (a turnstile): past it at 400");
    // ... the same barrier with no door past it: a closed pen
    TSceneInit(&s, 600.0f);
    TWall(&s.t, -700.0f, 700.0f, 300.0f, 320.0f, 120.0f);
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && cnt[TL_ROUTE] > 0, "through: past a barrier into a pen without a door: refused");
    // ... a low step of 30 (she walks over it): no obstacle at all
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 320.0f, 30.0f);
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && nObst == 0, "through: a 30 u step is no obstacle (the warp ahead applies)");
    // 5. a gap in the floor (a pit from 300 to 500, its floor 500 down): past it at 520 (the far floor at her level)
    TSceneInit(&s, 2000.0f);
    s.t.p[0].x0 = -2100.0f, s.t.p[0].x1 = 2100.0f, s.t.p[0].z0 = 300.0f, s.t.p[0].z1 = 500.0f, s.t.p[0].top = 500.0f, s.t.np = 1;
    TDoor(&s, 0.0f, 1900.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.kObst == 8 && t.k == 13 && Near(t.at[1], 0.0f, 0.1f) && cnt[TL_LEDGE] >= 4,
          "through: a pit 200 wide: its floor 500 down (another level, refused), the far side at 520");
    // 6. a pillar (60 x 60) straight ahead: past it at 440; its floor is hers (round the pillar)
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -30.0f, 30.0f, 300.0f, 360.0f, 1000.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.ray == 0 && t.k == 11 && s.m->seeds == 1, "through: a pillar ahead: past it (440); its floor: hers (no door needed)");
    // 7. the fan: a block 120 wide from 300 on to the room's end straight ahead (no spot on that ray): the +10 degree ray
    //    leaves its side and lands where her body clears it (x >= 60 + 70)
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -60.0f, 60.0f, 300.0f, 2100.0f, 1000.0f);
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.ray == 1 && t.k == 19 && t.at[0] >= 130.0f && Near(t.yaw, 0.0f, 1e-6f),
          "through: a long block straight ahead: the +10 degree ray's spot (her facing kept)");
    // 8. a walk-in event zone past the wall on the straight path (z 360 .. 1000, full width): refused on every ray
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    TDoor(&s, 0.0f, 1900.0f);
    s.z[0].x = 0.0f, s.z[0].y = 0.0f, s.z[0].z = 680.0f, s.z[0].shape = 2, s.z[0].sx = 4000.0f, s.z[0].sy = 300.0f, s.z[0].sz = 640.0f;
    s.nz = 1;
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(!ok && cnt[TL_ZONE] > 0 && cnt[TL_ROUTE] == 0, "through: a walk-in zone past the wall on the way: refused");
    // ... the same zone beyond the spot (z 600 .. 1000): the spot at 440 goes
    s.z[0].z = 800.0f, s.z[0].sz = 400.0f;
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(ok == 1 && t.k == 11, "through: a walk-in zone beyond the spot: the spot short of it goes");
    // 9. a door line in the wall (the next room past it): refused (through a doorway)
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    s.x[0].x0 = -500.0f, s.x[0].x1 = 500.0f, s.x[0].z0 = s.x[0].z1 = 320.0f, s.x[0].px = 0.0f, s.x[0].pz = 100.0f, s.nx = 1;
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && cnt[TL_DOOR] > 0, "through: a door line in the wall: never into the next room");
    // 10. no floor past the wall at all (the room's outside: the ground 9000 down): refused
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    s.t.p[0].x0 = -2100.0f, s.t.p[0].x1 = 2100.0f, s.t.p[0].z0 = 340.0f, s.t.p[0].z1 = 2100.0f, s.t.p[0].top = 9000.0f, s.t.np = 1;
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && cnt[TL_FLOOR] > 0 && cnt[TL_ROUTE] == 0, "through: no floor past the wall: refused");
    // ... another level past the wall (100 lower): refused
    s.t.p[0].top = 100.0f;
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst) && cnt[TL_LEDGE] > 0, "through: a floor 100 lower past the wall: refused (another level)");
    // 11. a low ceiling past the wall (a slab 250 above the floor): no head room
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -2100.0f, 2100.0f, 300.0f, 340.0f, 1000.0f);
    TDoor(&s, 0.0f, 1900.0f);
    s.t.b[s.t.nb].x0 = -2100.0f, s.t.b[s.t.nb].x1 = 2100.0f, s.t.b[s.t.nb].z0 = 340.0f, s.t.b[s.t.nb].z1 = 1000.0f;
    s.t.b[s.t.nb].y0 = -400.0f, s.t.b[s.t.nb].y1 = -250.0f, ++s.t.nb;
    ok = TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst);
    Check(!ok && cnt[TL_BODY] > 0, "through: a ceiling 250 above the floor past the wall: no head room, refused");
    // 12. a query that cannot answer: nothing is open, nothing lands (fail closed)
    TSceneInit(&s, 2000.0f);
    s.t.unknown = 1;
    Check(!TThru(&s, 0.0f, 800.0f, &t, cnt, &nObst), "through: the collision cannot answer: no spot");
    // 13. the samples: every 40 u up to the distance; off below 40; capped
    Check(GtThruSamples(800.0f) == 20 && GtThruSamples(820.0f) == 20 && GtThruSamples(39.0f) == 0 && GtThruSamples(0.0f) == 0 &&
          GtThruSamples(1e6f) == GT_THRU_SAMPLES && GtThruSamples(NAN) == 0, "through: the samples per ray");
    Check(Near(GtFanYaw(1.0f, 0), 1.0f, 1e-6f) && Near(GtFanYaw(1.0f, 1), 1.0f + GT_FAN_STEP, 1e-6f) &&
          Near(GtFanYaw(1.0f, 4), 1.0f - 2.0f * GT_FAN_STEP, 1e-6f), "through: the fan (0, +10, -10, +20, -20 degrees)");
}
// the room's floor map itself: stairs, a step too high, levels, the door line, the grid, the queue, built in parts
static void Reach(void)
{
    static TScene s;
    const float top[3] = { 0.0f, -240.0f, 1700.0f }, below[3] = { 0.0f, 0.0f, 600.0f };
    int k, n;
    // stairs up: treads 80 deep, risers 40, from z 1000 on (6 steps, the top floor 240 up); her feet below: the top is reached
    TSceneInit(&s, 2000.0f);
    for (k = 0; k < 6; ++k) TWall(&s.t, -2100.0f, 2100.0f, 1000.0f + 80.0f * (float)k, 2100.0f, 40.0f * (float)(k + 1));
    TMap(&s);
    Check(GtMapHas(s.m, &s.w, top) == GTM_HAS && GtMapHas(s.m, &s.w, below) == GTM_HAS, "map: up a flight of stairs (risers 40)");
    // risers of 60 (more than she climbs): the top is not reached
    TSceneInit(&s, 2000.0f);
    for (k = 0; k < 6; ++k) TWall(&s.t, -2100.0f, 2100.0f, 1000.0f + 80.0f * (float)k, 2100.0f, 60.0f * (float)(k + 1));
    TMap(&s);
    {
        const float top6[3] = { 0.0f, -360.0f, 1700.0f };
        Check(GtMapFinished(s.m) && GtMapHas(s.m, &s.w, top6) == GTM_NOT, "map: risers of 60: the top is not the room's floor");
        Check(GtMapHas(s.m, &s.w, (const float[3]){ 0.0f, -100.0f, 600.0f }) == GTM_NOT, "map: a spot at another level than the cell's: not held");
    }
    // a door line: the floor past it is not the room's (and a seed there is refused)
    TSceneInit(&s, 2000.0f);
    s.x[0].x0 = -2100.0f, s.x[0].x1 = 2100.0f, s.x[0].z0 = s.x[0].z1 = 1000.0f, s.x[0].px = 0.0f, s.x[0].pz = 800.0f, s.nx = 1;
    TMap(&s);
    Check(GtMapHas(s.m, &s.w, (const float[3]){ 0.0f, 0.0f, 1200.0f }) == GTM_NOT && GtMapHas(s.m, &s.w, below) == GTM_HAS,
          "map: never past one of the room's door lines");
    Check(GtMapSeed(s.m, &s.w, 0.0f, 0.0f, 1200.0f, GT_REACH_UP) == -1 && GtMapSeed(s.m, &s.w, 0.0f, 0.0f, 600.0f, GT_REACH_UP) == 0,
          "map: a seed past a door line is refused; one on the map's floor adds nothing");
    Check(GtExitBeyond(s.x, 1, (const float[3]){ 0.0f, 0.0f, 1100.0f }) && !GtExitBeyond(s.x, 1, (const float[3]){ 0.0f, 0.0f, 900.0f }) &&
          !GtExitBeyond(s.x, 1, (const float[3]){ 0.0f, -900.0f, 1100.0f }), "map: the far side of a door line (at its level only)");
    // a seed without a floor; off the grid; no map without a box
    TSceneInit(&s, 2000.0f);
    s.t.p[0].x0 = 500.0f, s.t.p[0].x1 = 700.0f, s.t.p[0].z0 = 500.0f, s.t.p[0].z1 = 700.0f, s.t.p[0].top = 9000.0f, s.t.np = 1;
    TMap(&s);
    Check(GtMapSeed(s.m, &s.w, 600.0f, 0.0f, 600.0f, GT_REACH_UP) == -1 && GtMapSeed(s.m, &s.w, 9000.0f, 0.0f, 0.0f, GT_REACH_UP) == -1 &&
          s.m->seedsNoFloor == 2 && GtMapHas(s.m, &s.w, (const float[3]){ 9000.0f, 0.0f, 0.0f }) == GTM_NONE,
          "map: seeds without a floor or off the grid: none; a spot off the grid: no map");
    // a spot whose own cell's centre (100, 40) is inside a post: not in the map; its neighbour (100, -20) is
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, 90.0f, 110.0f, 30.0f, 50.0f, 1000.0f);
    TMap(&s);
    Check(!s.m->st[GtMapIdx(s.m, 100.0f, 12.0f)] && GtMapHas(s.m, &s.w, (const float[3]){ 100.0f, 0.0f, 12.0f }) == GTM_HAS,
          "map: a spot whose own cell is a post's: a neighbour cell it can step to holds it");
    // built in parts: 100 cells a run until finished; the result the same
    TSceneInit(&s, 1000.0f);
    s.mapCells = 0;
    TMap(&s);
    for (n = 0; n < 1000 && !GtMapFinished(s.m); ++n) GtMapRun(s.m, &s.w, 100);
    Check(GtMapFinished(s.m) && s.m->done == s.m->cells && s.m->cells >= 33 * 33 && n > 5,
          "map: built 100 cells at a time over several ticks: the whole floor");
    // a big room: the grid's cells grow (at most 256 each way)
    {
        const float lo[3] = { -15000.0f, -100.0f, -15000.0f }, hi[3] = { 15000.0f, 100.0f, 15000.0f };
        GtMapBegin(s.m, lo, hi, NULL, 0);
        Check(s.m->on && s.m->nx <= GT_MAP_DIM && s.m->ny <= GT_MAP_DIM && s.m->cell > 117.0f && s.m->cell < 119.0f,
              "map: a room 30000 wide: 256 cells of 118 u");
        GtMapBegin(s.m, hi, lo, NULL, 0);
        Check(!s.m->on && GtMapHas(s.m, &s.w, below) == GTM_NONE, "map: a box with no size: no map");
    }
    // the walk step: a wall between two cells at any of the three heights blocks
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, -100.0f, 100.0f, 29.0f, 31.0f, 1000.0f);
    {
        float fy;
        int unk = 0;
        Check(!GtWalkStep(&s.w, (const float[3]){ 0.0f, 0.0f, 0.0f }, 0.0f, 60.0f, &fy, &unk) &&
              GtWalkStep(&s.w, (const float[3]){ 300.0f, 0.0f, 0.0f }, 300.0f, 60.0f, &fy, &unk) && unk == 0,
              "map: a thin wall between two cells blocks the step; elsewhere it goes");
    }
    // her body: the three spheres
    TSceneInit(&s, 2000.0f);
    TWall(&s.t, 100.0f, 200.0f, -50.0f, 50.0f, 1000.0f);
    Check(GtBodyFree(&s.w, 0.0f, 0.0f, 0.0f) && !GtBodyFree(&s.w, 50.0f, 0.0f, 0.0f), "body: 100 u from a wall free, 50 u not");
}
// A2: the enemy she faces
static void Faced(void)
{
    GtCand c[6];
    const float p[3] = { 0.0f, 0.0f, 0.0f };
    const float ahead[3] = { 0.0f, 0.0f, 500.0f }, right[3] = { 500.0f, 0.0f, 0.0f }, behind[3] = { 0.0f, 0.0f, -500.0f };
    const float d30[3] = { 250.0f, 0.0f, 433.0f };
    memset(c, 0, sizeof c);
    Check(Near(GtOffFacing(p, 0.0f, ahead), 0.0f, 1e-4f) && Near(GtOffFacing(p, 0.0f, right), 1.5707963f, 1e-4f) &&
          Near(GtOffFacing(p, 0.0f, behind), 3.1415926f, 1e-3f) && Near(GtOffFacing(p, 0.0f, d30), 30.0f * GT_DEG, 1e-3f) &&
          Near(GtOffFacing(p, 1.5707963f, right), 0.0f, 1e-3f) && Near(GtOffFacing(p, 0.0f, p), 0.0f, 0.0f) &&
          GtOffFacing(p, NAN, ahead) > 3.0f, "faced: the angle off her facing (ahead 0, right 90, behind 180, NaN never)");
    // the nearest enemy is behind her, the faced one 20 degrees off and far: the faced one, never the nearest
    c[0].d = 150.0f, c[0].ok = 1, c[0].ang = 170.0f * GT_DEG;
    c[1].d = 3000.0f, c[1].ok = 1, c[1].ang = 20.0f * GT_DEG;
    c[2].d = 900.0f, c[2].ok = 1, c[2].ang = 50.0f * GT_DEG;   // outside the 35-degree cone
    Check(GtPickFaced(c, 3, GT_RANGE_DEF, GT_CONE_DEF) == 1 && GtConeCount(c, 3, GT_RANGE_DEF, GT_CONE_DEF) == 1,
          "faced: the one in the cone (3000 u, 20 deg), not the nearest (behind her) nor one at 50 deg");
    Check(GtPickFaced(c, 3, 2500.0f, GT_CONE_DEF) == -1, "faced: v3.49's 2500 u would not reach it (A3: 5000 now)");
    Check(GtPickFaced(c, 3, GT_RANGE_DEF, 60.0f) == 1 && GtPickFaced(c, 1, GT_RANGE_DEF, GT_CONE_DEF) == -1,
          "faced: a wider cone still the smallest angle; only one behind her: none (the warp ahead)");
    // ties: within 5 degrees of the smallest angle the nearer; equal distance: the first
    c[3].d = 1200.0f, c[3].ok = 1, c[3].ang = 23.0f * GT_DEG;   // 3 degrees more, much nearer
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == 3, "faced: two within 5 degrees: the nearer");
    c[3].ang = 26.0f * GT_DEG;                                    // 6 degrees more: the smaller angle
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == 1, "faced: 6 degrees apart: the smaller angle, though farther");
    c[3].ang = 20.0f * GT_DEG, c[3].d = 3000.0f;
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == 1, "faced: the same angle and distance: the first in the list");
    // not standing, another floor, not in this room (beyond a door line / outside the box): never
    c[1].ok = 0;
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == 3, "faced: a downed enemy is skipped");
    c[3].dy = 700.0f;
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == -1, "faced: one on another floor is skipped");
    c[3].dy = 0.0f, c[3].skip = 1;
    Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == -1 && GtPickFaced(c, 0, GT_RANGE_DEF, GT_CONE_DEF) == -1,
          "faced: one marked not in this room is skipped; an empty list: none");
    c[4].d = 100.0f, c[4].ok = 1, c[4].ang = NAN;
    Check(GtPickFaced(c, 5, GT_RANGE_DEF, GT_CONE_DEF) == -1, "faced: an angle that is not a number: never");
    Check(GtConeKey(0, 0.0f) == GT_CONE_DEF && GtConeKey(1, -3.0f) == GT_CONE_DEF && GtConeKey(1, 2.0f) == GT_CONE_MIN &&
          GtConeKey(1, 200.0f) == GT_CONE_MAX && GtConeKey(1, 45.0f) == 45.0f, "faced: [Gameplay] GhostTeleportCone 35 (10 .. 90)");
}

// v3.51 TP351: the line from her to the enemy she faces against the room's collision; the cone's next enemy
typedef struct { const GtWorld *w; const float *her; const float (*e)[3]; const int *wall; int calls; } TSightCtx;
static int TSightTest(void *c, int i)  // teleport.inc GtFaceTest's sight half: 0 = in the open
{
    TSightCtx *x = (TSightCtx *)c;
    ++x->calls;
    return GtFoeSight(x->w, x->her, x->e[i], x->wall ? x->wall[i] : 0) == GTV_CLEAR ? 0 : 2;
}
static void TCands(const float her[3], const float (*e)[3], int n, GtCand *c)  // as GtFind fills them (standing, at her level)
{
    int i;
    memset(c, 0, sizeof(GtCand) * (size_t)n);
    for (i = 0; i < n; ++i) {
        c[i].ok = 1, c[i].dy = e[i][1] - her[1], c[i].ang = GtOffFacing(her, 0.0f, e[i]);
        c[i].d = sqrtf((e[i][0] - her[0]) * (e[i][0] - her[0]) + (e[i][2] - her[2]) * (e[i][2] - her[2]));
    }
}
static void Sight(void)
{
    static TScene s;
    const float her[3] = { 0.0f, 0.0f, 0.0f };
    // 1. open floor: a clear line (the v3.50 behaviour: behind it)
    TSceneInit(&s, 3000.0f);
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 800.0f }, 0) == GTV_CLEAR &&
          GtFoeSight(&s.w, her, (const float[3]){ 300.0f, 0.0f, 2500.0f }, 0) == GTV_CLEAR, "sight: open floor: in the open");
    // 2. a full wall between (z 300 .. 340, 1000 high): behind collision (her chest line)
    TWall(&s.t, -3100.0f, 3100.0f, 300.0f, 340.0f, 1000.0f);
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 800.0f }, 0) == GTV_CHEST &&
          GtFoeSight(&s.w, her, (const float[3]){ 200.0f, 0.0f, 360.0f }, 0) == GTV_CHEST, "sight: a wall between: behind "
          "collision (also one just past it)");
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 250.0f }, 0) == GTV_CLEAR, "sight: an enemy on her side of the wall, "
          "50 u from it (touching it): in the open");
    // 3. inside the wall (a ghost in it): its chest in solid
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 320.0f }, 0) == GTV_INSIDE, "sight: an enemy inside the wall: inside a wall");
    // 4. a thin turnstile (10 u, 120 high) between, on one level: her chest line passes over it, the low line does not
    TSceneInit(&s, 3000.0f);
    TWall(&s.t, -3100.0f, 3100.0f, 400.0f, 410.0f, 120.0f);
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_LOW, "sight: a thin turnstile (10 u, 120 high) "
          "between: behind collision (the low line)");
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 390.0f }, 0) == GTV_CLEAR, "sight: an enemy touching the turnstile "
          "on her side: in the open");
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, -30.0f, 900.0f }, 0) == GTV_LOW, "sight: the turnstile with the enemy 30 u "
          "up (one level, GT_FLOOR_TOL): the low line still tested");
    // 5. a counter 50 high: under both lines' reach? the low line (60 above) passes over it: in the open
    TSceneInit(&s, 3000.0f);
    TWall(&s.t, -3100.0f, 3100.0f, 400.0f, 440.0f, 50.0f);
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_CLEAR, "sight: a 50 u step / kerb between: in "
          "the open (she steps over it)");
    // 6. a window: a wall with a hole from 100 to 250 above the floor: the chest line passes the hole, the low line hits
    TSceneInit(&s, 3000.0f);
    {
        TBox *b = &s.t.b[s.t.nb++];
        b->x0 = -3100.0f, b->x1 = 3100.0f, b->z0 = 400.0f, b->z1 = 440.0f, b->y0 = -100.0f, b->y1 = 0.0f;   // below the hole
        b = &s.t.b[s.t.nb++];
        b->x0 = -3100.0f, b->x1 = 3100.0f, b->z0 = 400.0f, b->z1 = 440.0f, b->y0 = -1000.0f, b->y1 = -250.0f;  // above it
    }
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_LOW, "sight: an enemy seen through a window: "
          "behind collision");
    // 7. another level: a raised floor 100 u up from z 500 on (no barrier): the low line would clip its edge; the chest line only
    TSceneInit(&s, 3000.0f);
    {
        TBox *b = &s.t.b[s.t.nb++];
        b->x0 = -3100.0f, b->x1 = 3100.0f, b->z0 = 500.0f, b->z1 = 3100.0f, b->y0 = -100.0f, b->y1 = 0.0f;
    }
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, -100.0f, 1500.0f }, 0) == GTV_CLEAR, "sight: an enemy on a raised floor 100 u "
          "up (no barrier): in the open");
    Check(GtFoeSight(&s.w, (const float[3]){ 0.0f, -100.0f, 1500.0f }, (const float[3]){ 0.0f, 0.0f, 200.0f }, 0) == GTV_CLEAR,
          "sight: from the raised floor down to an enemy below it: in the open");
    Check(GtFoeSight(&s.w, (const float[3]){ 0.0f, -100.0f, 560.0f }, (const float[3]){ 0.0f, 0.0f, 450.0f }, 0) == GTV_CLEAR,
          "sight: down past the edge to one just below it: in the open (the chest line clears the edge)");
    TWall(&s.t, -3100.0f, 3100.0f, 2000.0f, 2040.0f, 1000.0f);  // a wall up there
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, -100.0f, 2500.0f }, 0) == GTV_CHEST, "sight: another level behind a wall: "
          "behind collision");
    // 8. a pillar beside the line: in the open; on it: behind collision
    TSceneInit(&s, 3000.0f);
    TWall(&s.t, 100.0f, 200.0f, 400.0f, 500.0f, 1000.0f);
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_CLEAR &&
          GtFoeSight(&s.w, her, (const float[3]){ 300.0f, 0.0f, 900.0f }, 0) == GTV_CHEST, "sight: a pillar beside the line / on it");
    // 9. a ghost in its wall state, a collision that cannot answer, values that are not numbers: never in the open
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 1) == GTV_WALLSTATE, "sight: a ghost with its wall portal "
          "open: in its wall");
    s.t.unknown = 1;
    Check(GtFoeSight(&s.w, her, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_UNKNOWN, "sight: a collision that cannot answer: "
          "not in the open (fail closed)");
    s.t.unknown = 0;
    Check(GtFoeSight(&s.w, her, (const float[3]){ NAN, 0.0f, 900.0f }, 0) == GTV_UNKNOWN &&
          GtFoeSight(&s.w, (const float[3]){ 0.0f, INFINITY, 0.0f }, (const float[3]){ 0.0f, 0.0f, 900.0f }, 0) == GTV_UNKNOWN,
          "sight: a position that is not a number: not in the open");
    Check(kGtSightWhy[GTV_CHEST] && kGtSightWhy[GTV_UNKNOWN] && GTV_N == 6, "sight: every reason has a text");
    // 10. the cone's next enemy: A straight ahead behind a wall, B 15 degrees off in the open, C behind her (never)
    TSceneInit(&s, 3000.0f);
    TWall(&s.t, -3100.0f, 40.0f, 300.0f, 340.0f, 1000.0f);  // a wall across her left and straight ahead (to x 40): B's line passes its end
    {
        const float e[4][3] = { { 0.0f, 0.0f, 900.0f }, { 268.0f, 0.0f, 1000.0f }, { 0.0f, 0.0f, -400.0f }, { -150.0f, 0.0f, 2000.0f } };
        const int wall[4] = { 0, 0, 0, 0 }, wallB[4] = { 0, 1, 0, 0 };
        GtCand c[4];
        TSightCtx x;
        int i;
        x.w = &s.w, x.her = her, x.e = e, x.wall = wall, x.calls = 0;
        TCands(her, e, 4, c);
        Check(GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) == 0, "next: v3.50 would take A (straight ahead, behind the wall)");
        i = GtPickFacedVia(c, 4, GT_RANGE_DEF, GT_CONE_DEF, TSightTest, &x);
        Check(i == 1 && c[0].skip && !c[1].skip && x.calls >= 2, "next: A behind collision is skipped: B (15 degrees off, in the "
              "open) is the enemy she faces");
        TCands(her, e, 4, c);
        x.wall = wallB, x.calls = 0;
        i = GtPickFacedVia(c, 4, GT_RANGE_DEF, GT_CONE_DEF, TSightTest, &x);
        Check(i == -1 && c[0].skip && c[1].skip && !c[2].skip, "next: A behind the wall, B a ghost in its wall, D (4 degrees, behind "
              "the wall too) skipped, C behind her never: none (the warp ahead)");
        TSceneInit(&s, 3000.0f);  // no wall: A in the open, as v3.50
        TCands(her, e, 4, c);
        x.wall = wall, x.calls = 0;
        Check(GtPickFacedVia(c, 4, GT_RANGE_DEF, GT_CONE_DEF, TSightTest, &x) == 0 && x.calls == 1, "next: all in the open: A, one "
              "test (v3.50's pick)");
        TCands(her, e, 4, c);
        Check(GtPickFacedVia(c, 4, GT_RANGE_DEF, GT_CONE_DEF, NULL, NULL) == GtPickFaced(c, 4, GT_RANGE_DEF, GT_CONE_DEF) &&
              GtPickFacedVia(c, 0, GT_RANGE_DEF, GT_CONE_DEF, TSightTest, &x) == -1, "next: no test = GtPickFaced; an empty list: none");
    }
}

// ================================================================ v3.52 TP352: every landing proven; freed from a wall
// v3.51's spot rules on the box world (teleport.inc GtSpotTest's order; the game's mover ray / room-to-stand: the floor
// query here), with or without the landing proof (GtLandProof)
enum { TS_OK, TS_BOX, TS_SIGHT, TS_WALL, TS_FLOOR, TS_LEDGE, TS_BODY, TS_ROUTE, TS_PENDING, TS_NOMAP, TS_N };
static int TSpot(TScene *s, const float from[3], const float c[3], int proof, float out[3])
{
    float a[3], b[3], fy;
    if (!GtInBox(s->lo, s->hi, c)) return TS_BOX;
    a[0] = from[0], a[1] = from[1] - GT_CHEST_DY, a[2] = from[2], b[0] = c[0], b[1] = from[1] - GT_CHEST_DY, b[2] = c[2];
    if (TLine(&s->t, a, b)) return TS_SIGHT;                        // the anchor's chest line (0x454000)
    if (TSolidAt(&s->t, c[0], from[1] - 4.0f, c[2])) return TS_WALL;  // 4 u above the floor not inside solid (0x46F700)
    if (!TFloor(&s->t, c[0], from[1] - 60.0f, from[1] + 6000.0f, c[2], &fy)) return TS_FLOOR;
    if (fabsf(fy - from[1]) > GT_FLOOR_TOL) return TS_LEDGE;
    out[0] = c[0], out[1] = fy, out[2] = c[2];
    if (!proof) return TS_OK;  // (v3.43 .. v3.51: the spot behind an enemy / ahead stopped here)
    switch (GtLandProof(s->m, &s->w, out)) {
    case GTP_OK: return TS_OK;
    case GTP_BODY: return TS_BODY;
    case GTP_ROUTE: return TS_ROUTE;
    case GTP_PENDING: return TS_PENDING;
    default: return TS_NOMAP;
    }
}
// the first of the 21 spots behind the enemy (e, back, rad) that passes; -1 none
static int TBehind(TScene *s, const float e[3], const float back[2], float rad, int proof, float out[3], int *cnt)
{
    int k;
    for (k = 0; k < GT_SPOT_N; ++k) {
        float c[3];
        int r;
        GtSpotCand(e, back, rad, k, c);
        if ((r = TSpot(s, e, c, proof, out)) == TS_OK) return k;
        if (cnt) ++cnt[r];
    }
    return -1;
}
// GtFreeCell checked against every cell of the map (the nearest that passes the same rules)
static float TFreeBrute(const GtMap *m, const GtWorld *w, const float from[3], float maxR)
{
    float best = 1e30f;
    int c;
    for (c = 0; c < m->nx * m->ny; ++c) {
        float x, z, fy, d2, p[3];
        if (!m->st[c] || !(fabsf((float)m->fy[c] - from[1]) <= GT_FREE_DY)) continue;
        GtMapXZ(m, c, &x, &z);
        d2 = (x - from[0]) * (x - from[0]) + (z - from[2]) * (z - from[2]);
        if (d2 > maxR * maxR || d2 >= best) continue;
        if (w->floorAt(w->ctx, x, (float)m->fy[c] - GT_REACH_UP, (float)m->fy[c] + GT_REACH_UP, z, &fy) <= 0 ||
            fabsf(fy - (float)m->fy[c]) > 2.0f || !GtBodyFree(w, x, fy, z))
            continue;
        p[0] = x, p[1] = fy, p[2] = z;
        if (GtStuck(w, p) != 0) continue;
        best = d2;
    }
    return best < 1e29f ? sqrtf(best) : -1.0f;
}
static int TOkLeft(void *c, const float p[3]) { return p[0] <= *(const float *)c; }  // a caller's rule: x at most this
static float TDistXZ(const float a[3], const float b[3]) { return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[2] - b[2]) * (a[2] - b[2])); }
static void Proof(void)
{
    static TScene s;
    // 1. warp 129 (the user's v3.51 play log, Water Prison 4:11, 09:49:10): "behind type 09 343 u away (the enemy she faces,
    //    1 degrees off her facing, 2 in the +-35 degree cone; spot 2: 160 u on its far side (no usable facing), floor +0 u of
    //    hers): from (-1752, 788) to (-1590, 1248)". Its body radius: 160 - GT_RING0 = 90; no usable facing: the far side
    //    from her (back = the way from her to it). The enemy's place solved from the two points: L - her = 343 b + 160 R(-30) b
    const float her[3] = { -1752.0f, 0.0f, 788.0f }, L[3] = { -1590.0f, 0.0f, 1248.0f };
    const float rad = 160.0f - GT_RING0, ca = cosf(-0.5235988f), sa = sinf(-0.5235988f);
    const float m00 = 343.0f + 160.0f * ca, m01 = -160.0f * sa;  // [X; Z] = [m00 m01; -m01 m00] [b0; b1]
    const float X = L[0] - her[0], Z = L[2] - her[2], det = m00 * m00 + m01 * m01;
    float back[2], e[3], c2[3], at[3], at2[3], q[3], l;
    int k0, k1, cnt[TS_N], tried, i;
    back[0] = (m00 * X - m01 * Z) / det, back[1] = (m01 * X + m00 * Z) / det;
    l = sqrtf(back[0] * back[0] + back[1] * back[1]), back[0] /= l, back[1] /= l;
    e[0] = her[0] + 343.0f * back[0], e[1] = 0.0f, e[2] = her[2] + 343.0f * back[1];
    GtSpotCand(e, back, rad, 2, c2);
    if (g_verbose) printf("  (warp 129: the enemy at (%.0f, %.0f), back (%.3f, %.3f), spot 2 at (%.1f, %.1f))\n", e[0], e[2], back[0], back[1], c2[0], c2[2]);
    Check(Near(c2[0], L[0], 3.0f) && Near(c2[2], L[2], 3.0f) && Near(GtOffFacing(her, atan2f(back[0], back[1]), e), 0.0f, 0.02f),
          "proof: warp 129 replayed: spot 2 of an enemy 343 u ahead (radius 90, its far side) is the logged landing (-1590, 1248)");
    //    The room: the Water Prison's wall 22 u past that landing (its face at z 1270, 130 thick, 1400 wide); her floor map
    //    from where she stood (no door needed: the room's floor is hers)
    TSceneInit(&s, 3000.0f);
    memcpy(s.her, her, sizeof s.her);
    TWall(&s.t, -2400.0f, -1000.0f, 1270.0f, 1400.0f, 1000.0f);
    TMap(&s);
    memset(cnt, 0, sizeof cnt);
    k0 = TBehind(&s, e, back, rad, 0, at, cnt);
    Check(k0 == 2 && Near(at[0], L[0], 3.0f) && Near(at[2], L[2], 3.0f) && cnt[TS_SIGHT] + cnt[TS_WALL] == 2,
          "proof: v3.51's spot rules: spots 0 and 1 in the wall (a wall between), spot 2 (22 u from its face) taken: warp 129's landing");
    Check(GtStuck(&s.w, at) == 1 && !GtBodyFree(&s.w, at[0], at[1], at[2]), "proof: at warp 129's landing her body overlaps the wall (stuck)");
    memset(cnt, 0, sizeof cnt);
    k1 = TBehind(&s, e, back, rad, 1, at2, cnt);
    if (g_verbose) printf("  (proven: spot %d at (%.0f, %.0f); refused: wall %d, body %d, route %d)\n", k1, at2[0], at2[2], cnt[TS_WALL], cnt[TS_BODY], cnt[TS_ROUTE]);
    Check(k1 > 2 && cnt[TS_BODY] >= 1 && GtBodyFree(&s.w, at2[0], at2[1], at2[2]) && GtStuck(&s.w, at2) == 0 &&
          1270.0f - at2[2] >= GT_BODY_HI_R && GtMapHas(s.m, &s.w, at2) == GTM_HAS,
          "proof: with the landing proof spot 2 is refused (no room for her body); a later spot clear of the wall is taken");
    //    after the warp (v3.51's landing): the nearest proven floor cell; the same answer as every cell of the map tested
    tried = 0;
    Check(GtFreeCell(s.m, &s.w, at, GT_FREE_R, NULL, NULL, q, &tried) && GtBodyFree(&s.w, q[0], q[1], q[2]) && GtStuck(&s.w, q) == 0 &&
          q[2] <= 1270.0f - GT_BODY_HI_R && TDistXZ(q, at) <= 120.0f && tried > 0 &&
          Near(TDistXZ(q, at), TFreeBrute(s.m, &s.w, at, GT_FREE_R), 1e-3f),
          "free: stuck at warp 129's landing: moved to the nearest proven floor cell (under 120 u, clear of the wall)");
    // 2. a warp that starts INSIDE the wall (50 u into it): stuck (inside solid), no floor seed there; the nearest cell
    {
        const float in[3] = { -1600.0f, 0.0f, 1320.0f }, face[3] = { -1500.0f, 0.0f, 1250.0f };
        const int seeds = s.m->seeds, cells = s.m->cells, blocked = s.m->seedsBlocked;
        Check(GtStuck(&s.w, in) == 1, "free: inside the wall: stuck");
        Check(GtMapSeed(s.m, &s.w, in[0], in[1], in[2], GT_REACH_UP) == -1 && s.m->seeds == seeds && s.m->cells == cells,
              "map: her feet inside the wall are no seed (no floor from inside solid)");
        Check(GtMapSeed(s.m, &s.w, face[0], face[1], face[2], GT_REACH_UP) == -1 && s.m->seedsBlocked == blocked + 1 &&
              s.m->cells == cells, "map: her feet 20 u from the wall's face (her body in it) are no seed either (inside collision)");
        Check(GtFreeCell(s.m, &s.w, in, GT_FREE_R, NULL, NULL, q, &tried) && q[2] <= 1270.0f - GT_BODY_HI_R &&
              TDistXZ(q, in) <= 180.0f && Near(TDistXZ(q, in), TFreeBrute(s.m, &s.w, in, GT_FREE_R), 1e-3f),
              "free: a warp from inside the wall: the nearest proven floor cell outside it");
        Check(GtStuck(&s.w, her) == 0 && GtStuck(&s.w, at2) == 0, "free: on open floor: not stuck");
    }
    // 3. the caller's rules, the reach, the level: a cell its rule refuses is never given; none within reach: none
    {
        float lim = -1700.0f, q2[3];
        Check(GtFreeCell(s.m, &s.w, at, GT_FREE_R, TOkLeft, &lim, q2, &tried) && q2[0] <= lim && TDistXZ(q2, at) >= TDistXZ(q, at) - 1e-3f,
              "free: the caller's rule (the door lines, zones, the game's floor ray) refuses cells: the nearest it allows");
        lim = -9000.0f;
        Check(!GtFreeCell(s.m, &s.w, at, GT_FREE_R, TOkLeft, &lim, q2, &tried), "free: every cell refused: none (she is not moved)");
        Check(!GtFreeCell(s.m, &s.w, (const float[3]){ -1600.0f, -400.0f, 1320.0f }, GT_FREE_R, NULL, NULL, q2, &tried),
              "free: from another level (400 u up): no cell of this floor");
        Check(!GtFreeCell(s.m, &s.w, (const float[3]){ 9000.0f, 0.0f, 9000.0f }, GT_FREE_R, NULL, NULL, q2, &tried) && tried == 0 &&
              !GtFreeCell(s.m, &s.w, (const float[3]){ NAN, 0.0f, 0.0f }, GT_FREE_R, NULL, NULL, q2, &tried),
              "free: far off the map, or not a number: none");
    }
    // 4. the warp ahead (v3.44): a wall face 30 u past 800 u ahead: v3.51 took 800 (her body in the wall); proven: 700
    TSceneInit(&s, 3000.0f);
    TWall(&s.t, -3100.0f, 3100.0f, 830.0f, 900.0f, 1000.0f);
    TMap(&s);
    {
        float c[3], o[3];
        GtFwdCand(s.her, 0.0f, 800.0f, 0, c);
        i = TSpot(&s, s.her, c, 0, o);
        k0 = TSpot(&s, s.her, c, 1, o);
        GtFwdCand(s.her, 0.0f, 800.0f, 1, c);
        k1 = TSpot(&s, s.her, c, 1, o);
        Check(i == TS_OK && k0 == TS_BODY && k1 == TS_OK && Near(o[2], 700.0f, 0.5f),
              "proof: the warp ahead 30 u short of a wall: v3.51 took it, now refused (her body); 700 u is taken");
    }
    // 5. the proof's answers: a closed pen past a wall (no route), an unfinished map (not known yet), no map / off it / NaN
    TSceneInit(&s, 600.0f);
    TWall(&s.t, -700.0f, 700.0f, 300.0f, 340.0f, 1000.0f);
    TWall(&s.t, -700.0f, 700.0f, 700.0f, 740.0f, 1000.0f);
    TMap(&s);
    Check(GtLandProof(s.m, &s.w, (const float[3]){ 0.0f, 0.0f, 520.0f }) == GTP_ROUTE &&
          GtLandProof(s.m, &s.w, (const float[3]){ 0.0f, 0.0f, 100.0f }) == GTP_OK &&
          GtLandProof(s.m, &s.w, (const float[3]){ 0.0f, 0.0f, 280.0f }) == GTP_BODY, "proof: a closed pen: no route; her side: proven; at the wall: her body");
    s.mapCells = 5;
    TMap(&s);
    Check(GtLandProof(s.m, &s.w, (const float[3]){ 0.0f, 0.0f, -500.0f }) == GTP_PENDING, "proof: the map not finished: not known yet");
    Check(GtLandProof(s.m, &s.w, (const float[3]){ 9000.0f, 0.0f, 0.0f }) == GTP_NOMAP &&
          GtLandProof(s.m, &s.w, (const float[3]){ 0.0f, NAN, 0.0f }) == GTP_NOMAP && GtLandProof(NULL, &s.w, s.her) == GTP_NOMAP,
          "proof: off the map, not a number, no map: not proven");
    s.m->on = 0;
    Check(GtLandProof(s.m, &s.w, s.her) == GTP_NOMAP, "proof: a map that could not begin (the room's box): not proven");
    Check(kGtProofWhy[GTP_NOMAP] && GTP_N == 5, "proof: every answer has a text");
    // 6. a query that cannot answer: never stuck on a guess; the solid map alone unknown: the sphere decides
    TSceneInit(&s, 600.0f);
    s.t.unknown = 1;
    Check(GtStuck(&s.w, s.her) == -1 && GtStuck(&s.w, (const float[3]){ INFINITY, 0.0f, 0.0f }) == -1, "free: the collision cannot answer: unknown (not moved)");
}
// the watch after a warp: stuck 0.15 s in a row frees her; clear for 1 s ends it; menus hold it; never past 3 s
static int TWatch(int hz, int stuckFrom, int stuckTo, int ticks, int *fireTick)  // stuck on ticks [stuckFrom, stuckTo)
{
    GtFreeWatch f;
    int i;
    *fireTick = -1;
    GtFreeWatchStart(&f);
    for (i = 0; i < ticks && f.on; ++i)
        if (GtFreeWatchStep(&f, 1, i >= stuckFrom && i < stuckTo, 1.0f / (float)hz) && *fireTick < 0) {
            *fireTick = i;
            GtFreeWatchStop(&f);
        }
    return i;
}
static void FreeWatch(void)
{
    static const int kHz[4] = { 30, 60, 144, 240 };
    int k, fire, n;
    for (k = 0; k < 4; ++k) {
        const int hz = kHz[k];
        char what[120];
        n = TWatch(hz, 0, 100000, 10 * hz, &fire);
        sprintf_s(what, sizeof what, "watch %d Hz: stuck from the landing: freed after 0.15 s (tick %d)", hz, fire);
        Check(fire >= 0 && fabsf((float)(fire + 1) / (float)hz - GT_FREE_STUCK_S) <= 1.0f / (float)hz + 1e-4f, what);
        n = TWatch(hz, -1, -1, 10 * hz, &fire);
        sprintf_s(what, sizeof what, "watch %d Hz: clear: the watch ends after 1 s (%d ticks), nobody moved", hz, n);
        Check(fire < 0 && abs(n - hz) <= 1, what);
        n = TWatch(hz, (int)(0.95f * (float)hz), 100000, 10 * hz, &fire);
        sprintf_s(what, sizeof what, "watch %d Hz: stuck from 0.95 s on: followed past the window, freed at %.2f s", hz, (float)(fire + 1) / (float)hz);
        Check(fire > hz && (float)(fire + 1) / (float)hz <= 0.95f + GT_FREE_STUCK_S + 2.0f / (float)hz, what);
        n = TWatch(hz, 0, (int)(0.1f * (float)hz), 10 * hz, &fire);
        sprintf_s(what, sizeof what, "watch %d Hz: stuck 0.1 s, then pushed out by the game: not moved", hz);
        Check(fire < 0, what);
    }
    {
        GtFreeWatch f;
        int i, fired = 0;
        GtFreeWatchStart(&f);
        for (i = 0; i < 600; ++i) fired |= GtFreeWatchStep(&f, 0, 1, 1.0f / 60.0f);  // a menu over it: nothing counts
        Check(!fired && f.on && f.t == 0.0f, "watch: menus / scenes hold it (no time, no stuck time)");
        for (i = 0; i < 6; ++i) fired |= GtFreeWatchStep(&f, 1, 1, 1.0f / 60.0f);
        fired |= GtFreeWatchStep(&f, 1, -1, 1.0f / 60.0f);  // an unknown answer breaks the run
        for (i = 0; i < 6; ++i) fired |= GtFreeWatchStep(&f, 1, 1, 1.0f / 60.0f);
        Check(!fired && f.on, "watch: a query that cannot answer breaks the stuck run (never moved on a guess)");
        GtFreeWatchStart(&f);
        for (i = 0; i < 1000 && f.on; ++i) fired |= GtFreeWatchStep(&f, 1, i & 1, 1.0f / 60.0f);  // flickering: never 0.15 s
        Check(!fired && !f.on && i >= (int)(GT_FREE_WATCH_S * 60.0f) && i <= (int)(GT_FREE_END_S * 60.0f) + 1,
              "watch: a stuck answer that flickers never frees her; the watch ends (at most 3 s)");
        GtFreeWatchStart(&f);
        for (i = 0; i < 100; ++i) fired |= GtFreeWatchStep(&f, 1, 1, 0.5f);  // a stall: no time passes
        Check(!fired && f.on && f.t == 0.0f, "watch: a stalled clock (0.5 s ticks): no time, no move");
        for (i = 0; i < 20; ++i) GtFreeWatchStep(&f, 1, 1, 1.0f / 60.0f);
        Check(GtFreeWatchStep(&f, 1, 1, 1.0f / 60.0f) == 1 && f.on, "watch: while she cannot be moved yet (a gate) it keeps asking");
        GtFreeWatchStop(&f);
        Check(!GtFreeWatchStep(&f, 1, 1, 1.0f / 60.0f), "watch: stopped: nothing");
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
    FadeDraw();  // v3.58 GT358
    Ahead();
    Pick();
    Spots();
    Rooms();  // GT343-001
    Keys();
    Hold();   // v3.49 GT349
    Zones();
    Through();  // v3.50 TP350
    Reach();
    Faced();
    Sight();  // v3.51 TP351
    Proof();  // v3.52 TP352
    FreeWatch();
    printf("%s: %d of %d checks passed\n", g_fail ? "FAILED" : "PASSED", g_n - g_fail, g_n);
    return g_fail ? 1 : 0;
}
