// v3.29 AFTERIMAGE: ai_sim.h native test (staging/special_mods/r3_afterimage/PLAN.md section 9). The pose ring read at
// the echo lags gives the same past poses at 30 / 60 / 120 / 144 / 180 / 240 frames a second (one tick and one drawn
// frame each, FrameMode 0); it keeps more than the Long trail at every rate; breaks reset it; the movement gate, the
// flicker, the capsule contact and the per-enemy cooldown (no stun-lock, the same at every rate); the enemy table.
// v3.30 AS330 (After Shock): the three lengths (Short 3 / 0.2 s, Medium 5 / 0.35 s = v3.29's On, Long 8 / 0.6 s) and the
// row's values; one contact per enemy whatever the echo count (Long never stuns more than Short, at every rate); the
// frame-time guard's ladder 8 -> 5 -> 3 and the spread of a capped trail.
// v3.33 AS333 (section 7): the copies in line (AfterImageFan 0 by default: no sideways shift), the cloak (AiBodyMeasure on
// Playable Eileen's and vanilla Henry's real bind skeletons: build 0 / 1, rigid under any pose; AiCloakBuild: widths from the
// active body, ellipse torso front / side, strength by the charge, a pure function of game time, finite when a limb points at
// the camera), the aura's level with its hysteresis, the frame guard's 5 s watch at 30 / 60 / 120 / 180 Hz, the build's cost.
// v3.34 FX334 (section 8; the user's v3.33 QA item 6: keep the cloak, remove the hard edges): no lit line (every lit ribbon is
// a fill as wide as her body there), every fill fades to ~0 at her outline (the strand profile at the measured outline,
// front and side, both bodies, stand / walk / run), both chain ends unlit, the outlines kept unlit for Thermal, no bright rim
// (the brightest point at a run), the same picture at the same game time at 30 / 60 / 120 / 180 Hz.
// v3.56 PW356 (section 10): the over-the-shoulder see-her fade (AiOtsBox / AiOtsK / AiOtsMixStep / AiOtsMinFromIni): both
// bodies' boxes, single copies in front of / behind / beside her, a running trail over her shoulder at 8 yaws (Henry and
// Eileen), the same trail from a side camera, the weight's fade at 60 / 180 Hz, the ini value.
// Build and run: test\build_ai_test.cmd (cl /O2 x86, as build.cmd).
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../src/ai_sim.h"

static int g_fail;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}
// her pelvis at game time t: a run along x (663 u/s, KB 63) with a side sway (curvature for the interpolation error)
static void Path(double t, float p[3])
{
    p[0] = (float)(663.0 * t);
    p[1] = -94.0f;
    p[2] = (float)(40.0 * sin(2.0 * 3.14159265358979 * 1.5 * t));
}
// one run of `secs` at hz frames a second: the clock advances 1 / hz per tick, the draw samples when due.
// Returns the echo pelvis x / z of the Normal and Long trails at the end, the samples taken and the ring's span.
typedef struct { float pos[AI_TRAILS][AI_ECHO_MAX][3]; int ok[AI_TRAILS][AI_ECHO_MAX], samples; double span, maxGap; } RunOut;
static AiRing g_r;
static void Run(int hz, double secs, RunOut *o)
{
    double now = 0.0;
    long k, n = (long)(secs * hz + 0.5);
    int tr, e, broke;
    memset(o, 0, sizeof *o);
    AiRingReset(&g_r);
    for (k = 0; k < n; ++k) {
        now += 1.0 / (double)(float)hz;  // the tick: + 1 / [0x5D5248] (a float division in the game's dt)
        if (AiRingDue(&g_r, now)) {
            float p[3];
            const int had = g_r.count;
            const double last = had ? g_r.stamp[AiRingNewest(&g_r)] : now;
            Path(now, p);
            AiRingPush(&g_r, now, p, &broke);
            if (broke) o->samples = -1000;
            if (had && now - last > o->maxGap) o->maxGap = now - last;
            ++o->samples;
        }
    }
    o->span = now - g_r.stamp[(g_r.head + AI_RING - g_r.count) % AI_RING];
    for (tr = 0; tr < AI_TRAILS; ++tr)
        for (e = 1; e <= kAiTrail[tr].n; ++e) {
            int i0, i1;
            float f;
            if (!AiRingAt(&g_r, now - AiLag(&kAiTrail[tr], e), &i0, &i1, &f)) continue;
            AiLerp3(o->pos[tr][e - 1], g_r.pelvis[i0], g_r.pelvis[i1], f);
            o->ok[tr][e - 1] = 1;
        }
}

// ---- 6. v3.32 AS332: the copies along her path, the gates, the fan, the near fade, the charge
// her pelvis paths (XZ; Y fixed): a straight run with sway (Path above), a walk, an about-turn on a 40 u radius, an
// instant reversal (a back-dodge then forward), a stop-start (run 0.5 s, stand 0.4 s), a tight circle
static void PWalk(double t, float p[3]) { p[0] = (float)(175.0 * t), p[1] = -94.0f, p[2] = 0.0f; }
static void PUturn(double t, float p[3])
{
    const double s = 663.0 * t, S1 = 663.0, R = 18.0, PI = 3.14159265358979;
    p[1] = -94.0f;
    if (s < S1) p[0] = (float)s, p[2] = 0.0f;
    else if (s < S1 + PI * R) p[0] = (float)(S1 + R * sin((s - S1) / R)), p[2] = (float)(R - R * cos((s - S1) / R));
    else p[0] = (float)(S1 - (s - S1 - PI * R)), p[2] = (float)(2.0 * R);
}
static void PReverse(double t, float p[3]) { p[0] = (float)(t < 1.2 ? 663.0 * t : 663.0 * 1.2 - 500.0 * (t - 1.2)), p[1] = -94.0f, p[2] = 0.0f; }
static void PStop(double t, float p[3])
{
    const double cyc = fmod(t, 0.9), n = floor(t / 0.9);
    p[0] = (float)(663.0 * (0.5 * n + (cyc < 0.5 ? cyc : 0.5))), p[1] = -94.0f, p[2] = 0.0f;
}
static void PCircle(double t, float p[3]) { p[0] = (float)(90.0 * cos(500.0 * t / 90.0)), p[1] = -94.0f, p[2] = (float)(90.0 * sin(500.0 * t / 90.0)); }
typedef void (*PathFn)(double, float[3]);
// a "break": between the first and the last copy that show (> 0.10), one (not inside her body) under 0.35 x the dimmer of
// the nearest showing copies before and after it (a lone gap or a run of them)
static int Broken(const float *g, const int *inside, int n)
{
    int k, a = -1, z = -1;
    for (k = 0; k < n; ++k)
        if (g[k] > 0.10f) z = k, a = a < 0 ? k : a;
    for (k = a + 1; a >= 0 && k < z; ++k) {
        int i = k - 1, j = k + 1;
        float lo;
        while (i > a && g[i] <= 0.10f) --i;
        while (j < z && g[j] <= 0.10f) ++j;
        lo = g[i] < g[j] ? g[i] : g[j];
        if (!inside[k] && g[k] < 0.35f * lo) return 1;
    }
    return 0;
}
typedef struct { int frames, brokeNew, brokeOld; float endPos[AI_ECHO_MAX][3]; int endOk[AI_ECHO_MAX]; float spaceRatio; } Cont;
static void Continuity(PathFn P, int hz, double secs, int tr, int ncap, Cont *o)
{
    const AiTrail *T = &kAiTrail[tr];
    const int n = ncap < T->n ? ncap : T->n;
    double now = 0.0;
    long q, steps = (long)(secs * hz + 0.5);
    float cum[AI_RING];
    memset(o, 0, sizeof *o);
    AiRingReset(&g_r);
    for (q = 0; q < steps; ++q) {
        float her[3], gN[AI_ECHO_MAX], gO[AI_ECHO_MAX], last[3], prevPath = 0.0f, smin = 1e9f, smax = 0.0f;
        int inN[AI_ECHO_MAX], inO[AI_ECHO_MAX], k, broke, have = 0;
        now += 1.0 / (double)(float)hz;
        P(now, her);
        if (AiRingDue(&g_r, now)) AiRingPush(&g_r, now, her, &broke);
        if (now < 0.8 || !AiRingFresh(&g_r, now)) continue;
        AiRingCum(&g_r, her, cum);
        ++o->frames;
        for (k = 1; k <= n; ++k) {  // v3.32: the copies along her path
            double t;
            float path, pel[3], f;
            int i0, i1;
            gN[k - 1] = 0.0f, inN[k - 1] = 0;
            if (!AiEchoAt(&g_r, cum, now, T->span, k, n, &t, &path) || !AiRingAt(&g_r, t, &i0, &i1, &f)) continue;
            AiLerp3(pel, g_r.pelvis[i0], g_r.pelvis[i1], f);
            gN[k - 1] = AiFall(k, n) * AiGate(path, AiDistXZ(pel, her));
            inN[k - 1] = AiDistXZ(pel, her) < AI_OVER1;
            if (gN[k - 1] > 0.05f) {
                const float sp = path - prevPath;
                if (sp < smin) smin = sp;
                if (sp > smax) smax = sp;
            }
            prevPath = path;
            if (q == steps - 1) memcpy(o->endPos[k - 1], pel, 12), o->endOk[k - 1] = 1;
        }
        for (k = 1; k <= n; ++k) {  // v3.31: time lags (spread when capped), the straight-distance gate, the stack fade
            const int kk = AiSpreadEcho(k, n, T->n);
            float pel[3], f, g;
            int i0, i1;
            gO[k - 1] = 0.0f, inO[k - 1] = 0;
            if (!AiRingAt(&g_r, now - AiLag(T, kk), &i0, &i1, &f)) continue;
            AiLerp3(pel, g_r.pelvis[i0], g_r.pelvis[i1], f);
            g = AiFall(kk, T->n) * AiMoveK(pel, her) * (have ? AiStackK(pel, last) : 1.0f);
            gO[k - 1] = g;
            inO[k - 1] = AiDistXZ(pel, her) < AI_OVER1;
            if (g >= 0.004f) memcpy(last, pel, 12), have = 1;
        }
        o->brokeNew += Broken(gN, inN, n);
        o->brokeOld += Broken(gO, inO, n);
        if (smax > 0.0f && smin < 1e8f && smax / (smin > 1e-3f ? smin : 1e-3f) > o->spaceRatio) o->spaceRatio = smax / (smin > 1e-3f ? smin : 1e-3f);
    }
}
static void Section6(void)
{
    static const int kR[4] = { 30, 60, 120, 180 };
    static const struct { PathFn p; const char *name; int oldBreaks; } kP[] = {
        { Path, "straight run (663 u/s, sway)", 0 }, { PWalk, "walk (175 u/s)", 0 }, { PUturn, "about-turn on an 18 u radius", 1 },
        { PReverse, "instant reversal (run, then back at 500 u/s)", 1 }, { PStop, "stop-start (run 0.5 s, stand 0.4 s)", 1 },
        { PCircle, "tight circle (90 u radius, 500 u/s)", 0 } };
    char b[300];
    int p, h, tr;
    float cum[AI_RING];
    for (p = 0; p < (int)(sizeof kP / sizeof kP[0]); ++p)
        for (tr = 1; tr < AI_TRAILS; ++tr) {
            int bn = 0, bo = 0, fr = 0;
            float worst = 0.0f, ratio = 0.0f;
            Cont c[4];
            for (h = 0; h < 4; ++h) {
                int k;
                Continuity(kP[p].p, kR[h], 3.0, tr, AI_ECHO_MAX, &c[h]);
                bn += c[h].brokeNew, bo += c[h].brokeOld, fr += c[h].frames;
                if (c[h].spaceRatio > ratio) ratio = c[h].spaceRatio;
                for (k = 0; k < kAiTrail[tr].n && h; ++k)
                    if (c[h].endOk[k] && c[1].endOk[k]) {
                        const float d = AiDistXZ(c[h].endPos[k], c[1].endPos[k]);
                        if (d > worst) worst = d;
                    }
            }
            sprintf(b, "continuity, %s, %s: broken frames v3.32 %d / v3.31 %d of %d (30/60/120/180 Hz); visible spacing max/min %.2f; "
                    "copies at 3 s within %.2f u of 60 Hz", kP[p].name, tr == 1 ? "Medium" : "Long", bn, bo, fr, ratio, worst);
            Check(bn == 0 && worst < 3.0f && (tr < 2 || !kP[p].oldBreaks || bo > 0), b);
        }
    {   // the capped trail: evenly along her path (v3.31's spread left a gap at her and paired two copies)
        Cont c;
        float lag[3], gapOld[3];
        int k;
        Continuity(Path, 60, 3.0, 2, 3, &c);
        for (k = 1; k <= 3; ++k) lag[k - 1] = AiLag(&kAiTrail[1], AiSpreadEcho(k, 3, 5));
        gapOld[0] = lag[0], gapOld[1] = lag[1] - lag[0], gapOld[2] = lag[2] - lag[1];
        sprintf(b, "capped (Long -> 3 copies): v3.32 visible spacing max/min %.2f, %d broken frames; v3.31's Medium -> 3 lags %.3f / %.3f / %.3f s "
                "(gaps %.3f %.3f %.3f)", c.spaceRatio, c.brokeNew, lag[0], lag[1], lag[2], gapOld[0], gapOld[1], gapOld[2]);
        Check(c.spaceRatio < 1.25f && c.brokeNew == 0 && gapOld[0] > 1.9f * gapOld[2], b);
    }
    {   // AiEchoAt on a known ring: a straight 600 u/s line sampled at 60 Hz for 1 s
        float her[3] = { 600.0f, 0.0f, 0.0f }, path;
        double t;
        int k, broke, ok = 1;
        AiRingReset(&g_r);
        for (k = 0; k <= 60; ++k) {
            const float pp[3] = { 10.0f * (float)k, 0.0f, 0.0f };
            AiRingPush(&g_r, k / 60.0, pp, &broke);
        }
        AiRingCum(&g_r, her, cum);
        for (k = 1; k <= 5; ++k) {
            ok &= AiEchoAt(&g_r, cum, 1.0, 0.35f, k, 5, &t, &path) && fabs(t - (1.0 - 0.07 * k)) < 1e-4 && fabsf(path - 42.0f * (float)k) < 0.01f;
        }
        Check(ok, "path spacing: on a steady line the copies are at v3.31's time lags (0.07 s apart, 42 u at 600 u/s)");
        Check(fabsf(AiPathAt(&g_r, cum, 0.5) - 300.0f) < 0.01f && AiPathAt(&g_r, cum, -1.0) < 0.0f, "path: 300 u back at 0.5 s; none before the ring");
        Check(fabsf(AiRingSpeed(&g_r, cum, 1.0) - 600.0f) < 0.5f, "speed: 600 u/s from the last 0.1 s of path");
        Check(AiRingFresh(&g_r, 1.04) && !AiRingFresh(&g_r, 1.06), "stale: a ring not filled for over 0.05 s draws no copies");
        for (k = 0; k <= 60; ++k) {  // a pause: 0.5 s on one spot, then 0.5 s of the line again
            const float pp[3] = { k < 30 ? 0.0f : 20.0f * (float)(k - 30), 0.0f, 0.0f };
            AiRingPush(&g_r, 2.0 + k / 60.0, pp, &broke);
        }
        her[0] = 600.0f;
        AiRingCum(&g_r, her, cum);
        ok = 1;
        {
            double prevT = 99.0;
            for (k = 1; k <= 8; ++k) {
                ok &= AiEchoAt(&g_r, cum, 3.0, 0.6f, k, 8, &t, &path) && t < prevT;
                prevT = t;
            }
        }
        Check(ok, "a pause inside the span: every copy at its own moment (none piled where she stood)");
    }
    // the gates
    Check(AiGate(10.0f, 100.0f) == 0.0f && AiGate(80.0f, 100.0f) == 1.0f && AiGate(80.0f, 5.0f) == 0.0f && AiGate(80.0f, 31.0f) == 1.0f,
          "gate: the path 20 -> 70 u; a copy within 10 u of her pelvis is inside her (full from 30 u)");
    // the fan
    {
        float mx = 0.0f, prev = 0.0f, x;
        int first = 1;
        Check(AiFanShift(0.0f, 5.0f, 1.0f) == 0.0f && fabsf(AiFanShift(-200.0f, 0.0f, 1.0f) - 100.0f) < 1e-3f &&
              fabsf(AiFanShift(150.0f, 0.0f, -1.0f) + 75.0f) < 1e-3f && AiFanShift(-200.0f, 120.0f, 1.0f) == 0.0f &&
              AiFanShift(-200.0f, -400.0f, 1.0f) == 0.0f && fabsf(AiFanShift(-200.0f, 0.0f, 0.5f) - 50.0f) < 1e-3f,
              "fan: side view untouched; on the axis 200 u toward the lens -> 100 u aside (either way); already aside or far on the "
              "other side: untouched; half-way through a side swing: half");
        for (x = -400.0f; x <= 400.0f; x += 0.5f) {  // continuous in the lateral position (no jump across the cone)
            const float v = x + AiFanShift(-200.0f, x, 1.0f);
            if (!first && fabsf(v - prev) > mx) mx = fabsf(v - prev);
            prev = v, first = 0;
        }
        sprintf(b, "fan: the moved position is continuous across the lateral axis (largest step %.2f u per 0.5 u)", mx);
        Check(mx < 2.0f, b);
    }
    for (h = 0; h < 4; ++h) {  // the side swing and the charge: the same at every rate (exact exponentials)
        float side = 1.0f, ch = AI_CHARGE_IDLE, ch2;
        const float dt = 1.0f / (float)kR[h];
        int k, n = kR[h] / 2;
        for (k = 0; k < n; ++k) side = AiFanSideStep(side, -60.0f, -200.0f, dt), ch = AiChargeStep(ch, 663.0f, dt);
        ch2 = ch;
        for (k = 0; k < n; ++k) ch2 = AiChargeStep(ch2, 0.0f, dt);
        sprintf(b, "%3d Hz: 0.5 s after the trail turns to the other side the fan's side is %.4f (exact %.4f); charge 0.3 -> %.4f at a "
                "run, %.4f 0.5 s after she stops", kR[h], side, -1.0f + 2.0f * expf(-0.5f / AI_FAN_TAU), ch, ch2);
        Check(fabsf(side - (-1.0f + 2.0f * expf(-0.5f / AI_FAN_TAU))) < 2e-4f && fabsf(ch - (1.0f - 0.7f * expf(-0.5f / AI_CHARGE_UP))) < 2e-4f &&
              fabsf(ch2 - (AI_CHARGE_IDLE + (ch - AI_CHARGE_IDLE) * expf(-0.5f / AI_CHARGE_DN))) < 2e-4f, b);
    }
    Check(AiFanSideStep(1.0f, 2.0f, -300.0f, 0.1f) == 1.0f && AiFanSideStep(-1.0f, 20.0f, -300.0f, 0.1f) == -1.0f,
          "fan side: holds while the tail runs (nearly) straight at the lens");
    // the near fade
    Check(AiNearK(800.0f, 800.0f) == 1.0f && AiNearK(490.0f, 800.0f) == 1.0f && AiNearK(270.0f, 800.0f) == 0.0f && AiNearK(-10.0f, 300.0f) == 0.0f &&
          AiNearK(40.0f, 100.0f) == 0.0f && AiNearK(190.0f, 300.0f) == 1.0f && AiNearK(150.0f, 300.0f) > 0.3f && AiNearK(150.0f, 300.0f) < 0.7f,
          "near fade: relative to her depth (0.35 -> 0.60 x), never within 45 u of the lens, nothing behind it");
}

// ---- 7. v3.33 AS333
// the bind skeletons (game coordinates, Y down, feet on y 0; henry01 entry 170: vanilla Henry's 30-bone body and Playable
// Eileen's installed 36-bone body), in AIB order: head 11, neck 10, chest 9, pelvis 8, shoulders 18 / 13, elbows 19 / 14,
// wrists 20 / 15, hips 5 / 2, knees 6 / 3, ankles 7 / 4, toes 24 / 22
static const float kBindE[AIB_N][3] = {
    { 0, -307.3f, 6.1f }, { 0, -290.3f, 1.5f }, { 0, -239.1f, 7.9f }, { 0, -210.9f, 6.9f }, { -28.4f, -276.5f, -1.1f }, { 28.4f, -276.5f, -1.1f },
    { -73.1f, -277.1f, -4.9f }, { 72.9f, -277.1f, -4.9f }, { -126.7f, -276.7f, 0.8f }, { 126.5f, -276.7f, 0.8f }, { -12.1f, -178.2f, -0.9f },
    { 12.1f, -178.2f, -0.9f }, { -12.1f, -102.5f, 1.4f }, { 12.1f, -102.5f, 1.4f }, { -12.1f, -26.3f, -1.7f }, { 12.1f, -26.3f, -1.7f },
    { -12.1f, -8.4f, 7.0f }, { 12.1f, -8.4f, 7.0f } };
static const float kBindH[AIB_N][3] = {
    { 0, -317.1f, -7.0f }, { 0, -303.3f, -10.5f }, { 0, -260.0f, -8.0f }, { 0, -219.0f, -2.6f }, { -34, -284, -16 }, { 34, -284, -16 },
    { -90, -284, -18 }, { 90, -284, -18 }, { -144, -284, -16 }, { 144, -284, -16 }, { -18, -186, -10 }, { 18, -186, -10 },
    { -18, -108, -12 }, { 18, -108, -12 }, { -18, -24, -18 }, { 18, -24, -18 }, { -19, -6, 4 }, { 19, -6, 4 } };
static void Pose7(const float in[AIB_N][3], float yaw, float tx, float tz, float out[AIB_N][3])  // a rigid turn and move
{
    int i;
    for (i = 0; i < AIB_N; ++i) {
        out[i][0] = in[i][0] * cosf(yaw) + in[i][2] * sinf(yaw) + tx;
        out[i][1] = in[i][1];
        out[i][2] = -in[i][0] * sinf(yaw) + in[i][2] * cosf(yaw) + tz;
    }
}
static float RibSum(const AiCloak *c, int kind)
{
    float s = 0.0f;
    int r, i;
    for (r = 0; r < c->nRib; ++r)
        if (c->r[r].kind == kind)
            for (i = 0; i < c->r[r].n; ++i) s += c->r[r].K[i];
    return s;
}
// the spine's two edge cores at its chest point (control 3 -> point 6): their distance (the outline's width there)
static float ChestWidth(const AiCloak *c)
{
    const AiCkRib *e0 = NULL, *e1 = NULL;
    int r;
    for (r = 0; r < c->nRib; ++r)
        if (c->r[r].ch == 0 && c->r[r].kind == 1) {
            if (!e0) e0 = &c->r[r];
            else if (!e1) e1 = &c->r[r];
        }
    return e0 && e1 ? AiLen3(e0->P[6], e1->P[6]) : -1.0f;
}
static void Section7(void)
{
    static const int kR[4] = { 30, 60, 120, 180 };
    static AiCloak ck, ck2;
    char b[320];
    float ph[AIB_N][3];
    const float eyeF[3] = { 0.0f, -250.0f, 700.0f }, eyeS[3] = { 700.0f, -250.0f, 0.0f };
    AiBody be, bh, bp;
    int h, i, r, ok;
    // in line: the default width 0 moves no copy (AiFanVec's shift = width x AiFanShift), v3.32's 100 % brings the fan back
    Check(AiFanWidthK(AI_FAN_DEFAULT) == 0.0f && AiFanWidthK(100) == 1.0f && AiFanWidthK(-5) == 0.0f && AiFanWidthK(500) == 2.0f,
          "in line: AfterImageFan default 0 (no shift); 100 = v3.32's fan; clamped 0..200 %");
    {   // a run straight away from the camera (the view axis): every copy at its path point, none beside it
        float mx = 0.0f, mx332 = 0.0f;
        int k;
        for (k = 1; k <= 8; ++k) {
            const float along = -75.0f * (float)k, sh = AiFanWidthK(AI_FAN_DEFAULT) * AiFanShift(along, 0.0f, 1.0f);
            const float sh332 = AiFanWidthK(100) * AiFanShift(along, 0.0f, 1.0f);
            if (fabsf(sh) > mx) mx = fabsf(sh);
            if (fabsf(sh332) > mx332) mx332 = fabsf(sh332);
        }
        sprintf(b, "in line: running away from the camera, Long: the largest sideways shift %.2f u (v3.32's fan: %.0f u)", mx, mx332);
        Check(mx == 0.0f && mx332 > 250.0f, b);
    }
    // the body: Eileen build 0, Henry build 1, rigid under a turn and a move
    ok = AiBodyMeasure(kBindE, &be) && AiBodyMeasure(kBindH, &bh);
    sprintf(b, "body: Playable Eileen hips %.1f u, lengths x%.3f, build %.2f, radii x%.2f; Henry hips %.1f u, lengths x%.3f, build %.2f, "
            "radii x%.2f", be.hips, be.lenScale, be.build, be.sizeK, bh.hips, bh.lenScale, bh.build, bh.sizeK);
    Check(ok && fabsf(be.build) < 0.01f && fabsf(bh.build - 1.0f) < 0.01f && fabsf(be.sizeK - 1.0f) < 0.01f && fabsf(bh.sizeK - 1.0f) < 0.01f &&
          fabsf(bh.lenScale - 1.047f) < 0.01f, b);
    Pose7(kBindH, 2.3f, 1500.0f, -800.0f, ph);
    Check(AiBodyMeasure(ph, &bp) && fabsf(bp.build - bh.build) < 1e-3f && fabsf(bp.sizeK - bh.sizeK) < 1e-3f, "body: the same size after a turn and a move (rigid)");
    {
        float big[AIB_N][3];
        int j;
        for (i = 0; i < AIB_N; ++i)
            for (j = 0; j < 3; ++j) big[i][j] = kBindE[i][j] * 1.2f;
        ok = AiBodyMeasure(big, &bp);
        sprintf(b, "body: an Eileen 1.2 x larger: lengths x%.2f, build %.2f (wider hips), radii x%.2f", bp.lenScale, bp.build, bp.sizeK);
        Check(ok && fabsf(bp.lenScale - 1.2f) < 0.01f && bp.sizeK > 1.05f && bp.sizeK < 1.25f, b);
    }
    // the cloak from the front and the side, both bodies
    {
        float wEF, wES, wHF, wHS;
        int nE, nH, fin = 1, maxN = 0;
        nE = AiCloakBuild(kBindE, 1, eyeF, 1, 1.0f, 3.0, &ck);
        wEF = ChestWidth(&ck);
        for (r = 0; r < ck.nRib; ++r) {
            if (ck.r[r].n > maxN) maxN = ck.r[r].n;
            for (i = 0; i < ck.r[r].n; ++i) fin &= AiFinite3(ck.r[r].P[i]) && ck.r[r].W[i] > 0.0f && ck.r[r].K[i] >= 0.0f;
        }
        AiCloakBuild(kBindE, 1, eyeS, 1, 1.0f, 3.0, &ck2);
        wES = ChestWidth(&ck2);
        nH = AiCloakBuild(kBindH, 1, eyeF, 1, 1.0f, 3.0, &ck);
        wHF = ChestWidth(&ck);
        AiCloakBuild(kBindH, 1, eyeS, 1, 1.0f, 3.0, &ck2);
        wHS = ChestWidth(&ck2);
        sprintf(b, "cloak: %d / %d ribbons (Eileen / Henry; at most %d; v3.34: 6 fills + 11 outlines), %d points a ribbon at most (%d), "
                "all finite: %d", nE, nH, AI_CK_RIBS, maxN, AI_CK_PTS, fin);
        Check(nE == 17 && nH == 17 && ck.nFill == 6 && maxN <= AI_CK_PTS && fin, b);  // v3.34 FX334: 17 (v3.33: 28)
        sprintf(b, "cloak: her outline at the chest from the front / side: Eileen %.1f / %.1f u (mesh 2 x 25 / 2 x 21), Henry %.1f / %.1f u "
                "(2 x 40 / 2 x 26)", wEF, wES, wHF, wHS);
        Check(fabsf(wEF - 50.0f) < 4.0f && fabsf(wES - 42.0f) < 4.0f && fabsf(wHF - 80.0f) < 4.0f && fabsf(wHS - 52.0f) < 4.0f, b);
    }
    {   // strength by the charge: standing faint, walking stronger, running strong (game time fixed: the same crackle)
        float f[3], e[3];
        static const float kC[3] = { 0.3f, 0.67f, 1.0f };
        for (i = 0; i < 3; ++i) {
            AiCloakBuild(kBindE, 1, eyeF, 1, kC[i], 4.0, &ck);
            f[i] = RibSum(&ck, 0), e[i] = RibSum(&ck, 1);
        }
        sprintf(b, "cloak: fill strength standing %.2f, walking %.2f, running %.2f (summed over the points); the outlines %.2f / %.2f / %.2f "
                "(v3.34: never lit)", f[0], f[1], f[2], e[0], e[1], e[2]);
        Check(f[0] < f[1] && f[1] < f[2] && f[2] > 3.0f * f[0] && e[0] == 0.0f && e[1] == 0.0f && e[2] == 0.0f, b);  // v3.34 FX334
    }
    {   // a pure function of game time: the same picture at the same moment whatever the frame rate; the crackle holds 50 ms
        int same = 1, held = 1, moved = 0;
        AiCloakBuild(kBindE, 1, eyeF, 1, 0.8f, 7.012, &ck);
        AiCloakBuild(kBindE, 1, eyeF, 1, 0.8f, 7.012, &ck2);
        same = !memcmp(&ck, &ck2, sizeof ck);
        AiCloakBuild(kBindE, 1, eyeF, 1, 0.8f, 7.048, &ck2);
        for (r = 0; r < ck.nRib; ++r)
            for (i = 0; i < ck.r[r].n; ++i) held &= AiLen3(ck.r[r].P[i], ck2.r[r].P[i]) < 1e-4f;
        AiCloakBuild(kBindE, 1, eyeF, 1, 0.8f, 7.052, &ck2);
        for (r = 0; r < ck.nRib; ++r)
            if (ck.r[r].kind == 1)
                for (i = 0; i < ck.r[r].n; ++i) moved |= AiLen3(ck.r[r].P[i], ck2.r[r].P[i]) > 0.05f;
        Check(same && held && moved, "cloak: a pure function of game time (any frame rate draws the same); the crackle re-rolls every 50 ms");
    }
    {   // a limb pointing straight at the camera, no eye, no toes, a broken bone
        float eye[3], bad[AIB_N][3];
        int fin = 1;
        for (i = 0; i < 3; ++i) eye[i] = kBindE[AIB_LHAND][i] + (kBindE[AIB_LHAND][i] - kBindE[AIB_LELB][i]) * 12.0f;
        AiCloakBuild(kBindE, 1, eye, 1, 1.0f, 1.0, &ck);
        for (r = 0; r < ck.nRib; ++r)
            for (i = 0; i < ck.r[r].n; ++i) fin &= AiFinite3(ck.r[r].P[i]);
        AiCloakBuild(kBindE, 0, eye, 0, 1.0f, 1.0, &ck2);
        for (r = 0; r < ck2.nRib; ++r)
            for (i = 0; i < ck2.r[r].n; ++i) fin &= AiFinite3(ck2.r[r].P[i]);
        memcpy(bad, kBindE, sizeof bad);
        bad[AIB_LKNEE][1] = (float)(1e30 * 1e30);
        Check(fin && ck.nRib == 17 && ck2.nRib == 17 && AiCloakBuild(bad, 1, eyeF, 1, 1.0f, 1.0, &ck) == 0,  // v3.34 FX334: 17
              "cloak: finite with an arm pointing at the camera, without an eye or toe joints; a non-finite bone builds nothing");
    }
    // the aura's level: bands and hysteresis; the steady charge of a stand / walk / run at every rate
    Check(AiAuraBand(-1, 0.3f) == 0 && AiAuraBand(0, 0.47f) == 0 && AiAuraBand(0, 0.50f) == 1 && AiAuraBand(1, 0.43f) == 1 &&
          AiAuraBand(1, 0.40f) == 0 && AiAuraBand(1, 0.88f) == 1 && AiAuraBand(1, 0.90f) == 2 && AiAuraBand(2, 0.83f) == 2 &&
          AiAuraBand(2, 0.80f) == 1 && AiAuraBand(0, 1.0f) == 2 && AiAuraBand(2, 0.3f) == 0, "aura level: standing < 0.45 <= walking < 0.85 <= running, 0.04 hysteresis");
    for (h = 0; h < 4; ++h) {
        static const float kV[3] = { 0.0f, 175.0f, 663.0f };
        int band[3], k, v;
        for (v = 0; v < 3; ++v) {
            float c = AI_CHARGE_IDLE;
            int bd = -1;
            for (k = 0; k < kR[h] * 2; ++k) c = AiChargeStep(c, kV[v], 1.0f / (float)kR[h]), bd = AiAuraBand(bd, c);
            band[v] = bd;
        }
        sprintf(b, "%3d Hz: aura level after 2 s standing / walking (175 u/s) / running (663 u/s): %d / %d / %d", kR[h], band[0], band[1], band[2]);
        Check(band[0] == 0 && band[1] == 1 && band[2] == 2, b);
    }
    // the frame guard's watch: the trail held under its setting from t = 1 s: one line (v3.33 MERGE: LogV) at 6 s of game time at every rate
    for (h = 0; h < 4; ++h) {
        AiCutW w;
        double now = 0.0, fireT = -1.0;
        int k, fires = 0;
        AiCutReset(&w);
        for (k = 0; k < kR[h] * 12; ++k) {
            now += 1.0 / (double)(float)kR[h];
            if (AiCutWatch(&w, now >= 1.0 && !(now >= 8.0 && now < 8.5), now)) ++fires, fireT = now;
        }
        sprintf(b, "%3d Hz: the guard's watch fired %d time(s), at %.3f s (held from 1 s; a release at 8 s does not fire it again)", kR[h], fires, fireT);
        Check(fires == 1 && fireT >= 6.0 - 1e-6 && fireT < 6.0 + 1.0 / kR[h] + 1e-6, b);
    }
    {   // the cost of one build (CPU, this machine)
        const int N = 20000;
        clock_t c0 = clock();
        float sink = 0.0f;
        for (i = 0; i < N; ++i) {
            AiCloakBuild(kBindE, 1, eyeF, 1, 0.9f, 1.0 + 0.001 * i, &ck);
            sink += ck.r[0].K[0];
        }
        {
            const double us = (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / N;
            sprintf(b, "cloak: one build %.1f us (%d ribbons, %d drawn, %d points; %.3f)", us, ck.nRib, ck.nFill, ck.nPts, sink * 0.0f);
            Check(us < 100.0, b);
        }
    }
}

// ---- v3.34 FX334 (section 8): the cloak's soft edges
static float Strand(float u)  // deathpresence_draw.inc DpSmokeTexel, the DPS_STRAND cell across u: 1 - smoothstep(0.15, 1, |u|)
{
    float x = (fabsf(u) - 0.15f) / 0.85f;
    x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
    return 1.0f - x * x * (3.0f - 2.0f * x);
}
// the strand profile of chain ch's fill at her measured outline: half the distance between its two outline polylines at each
// inner point, averaged over 64 crackle epochs (the crackle is zero-mean); the largest value over the inner points (-1: the
// chain has no two outlines). bn, eye, c, t: the build (AiCloakBuild)
static float FillAtOutline(const float bn[AIB_N][3], const float eye[3], float c, double t, int ch, float *worstR, float *worstW)
{
    static AiCloak ck;
    float R[AI_CK_PTS], W[AI_CK_PTS], mx = -1.0f;
    int r, i, ep, n = 0;
    memset(R, 0, sizeof R);
    for (ep = 0; ep < 64; ++ep) {
        const AiCkRib *f = NULL, *o1 = NULL, *o2 = NULL;
        AiCloakBuild(bn, 1, eye, 1, c, t + 0.05 * ep, &ck);
        for (r = 0; r < ck.nRib; ++r) {
            const AiCkRib *q = &ck.r[r];
            if (q->ch != ch) continue;
            if (q->kind == 0) f = q;
            else if (q->kind == 1 && !o1) o1 = q;
            else if (q->kind == 1) o2 = q;
        }
        if (!f || !o1 || !o2) return -1.0f;
        n = f->n;
        for (i = 0; i < n; ++i) R[i] += 0.5f * AiLen3(o1->P[i], o2->P[i]) / 64.0f, W[i] = f->W[i];
    }
    for (i = 1; i + 1 < n; ++i) {
        const float v = Strand(R[i] / W[i]);
        if (v > mx) mx = v, *worstR = R[i], *worstW = W[i];
    }
    return mx;
}
static void Section8(void)
{
    static const float kC[3] = { 0.3f, 0.67f, 1.0f };
    static const int kR[4] = { 30, 60, 120, 180 };
    static AiCloak ck, ck2;
    const float eyes[2][3] = { { 0.0f, -250.0f, 700.0f }, { 700.0f, -250.0f, 0.0f } };
    char b[400];
    float worst = 0.0f, worstR = 0.0f, worstW = 0.0f, thin = 1e9f, peak = 0.0f, v333 = 1.0f;
    int body, cam, ci, r, i, ok = 1, ends = 1, lines = 0, outl = 1, mid = 1;
    for (body = 0; body < 2; ++body)
        for (cam = 0; cam < 2; ++cam)
            for (ci = 0; ci < 3; ++ci) {
                int ch;
                ok &= AiCloakBuild(body ? kBindH : kBindE, 1, eyes[cam], 1, kC[ci], 5.0 + 0.37 * ci, &ck) == 17 && ck.nFill == 6;
                for (r = 0; r < ck.nRib; ++r) {
                    const AiCkRib *q = &ck.r[r];
                    float km = 0.0f;
                    for (i = 0; i < q->n; ++i) km = q->K[i] > km ? q->K[i] : km;
                    if (q->kind != 0) {  // an outline: points only
                        outl &= km == 0.0f;
                        continue;
                    }
                    if (km > peak) peak = km;
                    ends &= q->K[0] == 0.0f && q->K[q->n - 1] == 0.0f;
                    for (i = 1; i + 1 < q->n; ++i) {
                        mid &= q->K[i] > 0.0f;
                        if (q->W[i] < thin) thin = q->W[i];
                    }
                }
                for (r = 0; r < ck.nRib; ++r) lines += ck.r[r].kind != 0 && ck.r[r].kind != 1;
                if (ci != 1) continue;  // (the outline does not depend on the charge but for its crackle: once a body and camera)
                for (ch = 0; ch < AI_CK_CH; ++ch) {
                    float wr = 0.0f, ww = 0.0f;
                    const float v = FillAtOutline(body ? kBindH : kBindE, eyes[cam], kC[ci], 5.0, ch, &wr, &ww);
                    if (v > worst) worst = v, worstR = wr, worstW = ww;
                    if (v >= 0.0f) {  // v3.33's width (1.3 R + 2 u; R from this fill: (W - pad) / 1.10) at the same outline: its glow there
                        const float g = ck.body.sizeK, v3 = Strand(wr / (1.30f * (ww - AI_CK_FILL_PAD * g) / AI_CK_FILL_W + 2.0f * g));
                        if (v3 < v333) v333 = v3;
                    }
                }
            }
    sprintf(b, "soft edges: Eileen and Henry, front and side, standing / walking / running: 17 ribbons, 6 of them drawn (fills), no "
            "other kind (v3.33's edge glows); the outlines never lit: %d", outl);
    Check(ok && !lines && outl, b);
    sprintf(b, "soft edges: no lit line: the thinnest lit strip is %.1f u half wide (v3.33's edge cores: 0.8 u, drawn >= 0.75 px)", thin);
    Check(thin > 4.0f, b);
    sprintf(b, "soft edges: each fill at her outline (measured from the outlines, the crackle averaged out) is at most %.3f of its centre "
            "(at a half extent of %.1f u, fill %.1f u; v3.33's wider fill: >= %.2f there, its glow ran past her outline)", worst, worstR,
            worstW, v333);
    Check(worst >= 0.0f && worst < 0.10f && v333 > 0.18f, b);
    Check(ends && mid, "soft edges: both end points of every fill unlit (no square strip end at the crown, fingertips, toes or "
                       "shoulders), every inner point lit");
    sprintf(b, "soft edges: the brightest point of any fill at a run %.2f (no bright rim; v3.33's edge cores reached ~1.0)", peak);
    Check(peak > 0.2f && peak < 0.55f, b);
    {   // a pure function of game time: the same frame at 30 / 60 / 120 / 180 Hz (the clock advanced in frame steps to 7 / 3 s,
        // a whole number of frames at each rate, inside a crackle epoch)
        int same = 1, h;
        AiCloakBuild(kBindE, 1, eyes[0], 1, 0.8f, 7.0 / 3.0, &ck);
        for (h = 0; h < 4; ++h) {
            double t = 0.0;
            int k;
            for (k = 0; k < kR[h] * 7 / 3; ++k) t += 1.0 / (double)kR[h];
            AiCloakBuild(kBindE, 1, eyes[0], 1, 0.8f, t, &ck2);
            for (r = 0; r < ck.nRib; ++r)
                for (i = 0; i < ck.r[r].n; ++i)
                    same &= fabsf(ck.r[r].K[i] - ck2.r[r].K[i]) < 1e-3f && AiLen3(ck.r[r].P[i], ck2.r[r].P[i]) < 1e-2f;
        }
        Check(same, "soft edges: 30 / 60 / 120 / 180 Hz reach the same cloak at 2.33 s of game time");
    }
}

// v3.35 FX335 (section 9): no cloak drawn outside Thermal; afterimage.inc AiCloakMake measures her body with AiBodyMeasure
// alone there: the arcs' and tendrils' girth (build, radii) must be exactly the cloak build's, both bodies, any pose, and the
// Thermal outline (the hot points) must still come from AiCloakBuild's unlit outline points
static void Section9(void)
{
    static AiCloak ck;
    const float eye[3] = { 0.0f, -250.0f, 700.0f };
    float p[AIB_N][3];
    char b[256];
    int body, k, same = 1, outl = 1, lit = 0;
    for (body = 0; body < 2; ++body)
        for (k = 0; k < 4; ++k) {
            AiBody m;
            int r;
            Pose7(body ? kBindH : kBindE, 0.7f * (float)k, 300.0f * (float)k, -150.0f * (float)k, p);
            same &= AiBodyMeasure(p, &m) && AiCloakBuild(p, 1, eye, 1, 0.3f + 0.2f * (float)k, 2.0 + k, &ck) > 0 &&
                    m.build == ck.body.build && m.sizeK == ck.body.sizeK && m.hips == ck.body.hips && m.lenScale == ck.body.lenScale;
            for (r = 0; r < ck.nRib; ++r) {
                int i;
                if (ck.r[r].kind == 1) outl &= ck.r[r].n >= 2;
                for (i = 0; i < ck.r[r].n; ++i) lit += ck.r[r].kind == 1 && ck.r[r].K[i] > 0.0f;
            }
        }
    sprintf(b, "no cloak: AiBodyMeasure gives the cloak build's body exactly (Eileen and Henry, 4 poses each): %s", same ? "yes" : "NO");
    Check(same, b);
    Check(outl && lit == 0, "no cloak: Thermal's hot points still have her unlit outline points (every outline has points, none lit)");
}


// ---------------------------------------------------------------- v3.56 PW356 (section 10): the over-the-shoulder see-her fade
// her box from her bones (Eileen: hips 24 u apart, head bone 307 u, ankles 26 u above the floor; Henry: 36, 317, 24), the
// fade of a copy by where it is drawn (between the camera and her and over her on screen: down to minK; beside her or
// behind her: none), a running trail seen over her shoulder (the near copies over her fade, the ones beside or past the
// camera keep their light), the same trail from a side camera (none fades), Henry's broader box, the weight's fade at
// 60 / 180 Hz, the ini value.
static void OtsBones(int henry, const float at[3], float yaw, float head[3], float lf[3], float rf[3], float lh[3], float rh[3])
{
    const float hy = henry ? -317.0f : -307.3f, ay = henry ? -24.0f : -26.3f, hip = henry ? 18.0f : 12.05f, hipY = henry ? -186.0f : -178.2f;
    const float cx = cosf(yaw), cz = -sinf(yaw);  // her right (across her facing: forward is (sin, 0, cos))
    head[0] = at[0], head[1] = at[1] + hy, head[2] = at[2];
    lf[0] = at[0] - cx * 10.0f, lf[1] = at[1] + ay, lf[2] = at[2] - cz * 10.0f;
    rf[0] = at[0] + cx * 10.0f, rf[1] = at[1] + ay, rf[2] = at[2] + cz * 10.0f;
    lh[0] = at[0] - cx * hip, lh[1] = at[1] + hipY, lh[2] = at[2] - cz * hip;
    rh[0] = at[0] + cx * hip, rh[1] = at[1] + hipY, rh[2] = at[2] + cz * hip;
}
static void Section10(void)
{
    const float org[3] = { 0.0f, 0.0f, 0.0f };
    float head[3], lf[3], rf[3], lh[3], rh[3], c[3], W, H, We, He, Wh, Hh, k, prev;
    char b[256];
    int body, i, ok, mono, bad;
    // the boxes
    OtsBones(0, org, 0.0f, head, lf, rf, lh, rh);
    ok = AiOtsBox(head, lf, rf, lh, rh, c, &We, &He);
    sprintf(b, "ots: Eileen's box %.1f x %.1f u (hips 24.1 + 22 = 46 half, half the head-ankle span + 30 = 170.5), centre y %.1f", 2 * We, 2 * He, c[1]);
    Check(ok && fabsf(We - 46.1f) < 0.5f && fabsf(He - 170.5f) < 0.5f && fabsf(c[1] + 166.8f) < 0.5f, b);
    OtsBones(1, org, 0.0f, head, lf, rf, lh, rh);
    ok = AiOtsBox(head, lf, rf, lh, rh, c, &Wh, &Hh);
    sprintf(b, "ots: Henry's box %.1f x %.1f u (broader: hips 36)", 2 * Wh, 2 * Hh);
    Check(ok && fabsf(Wh - 58.0f) < 0.5f && fabsf(Hh - 176.5f) < 0.5f && Wh > We + 10.0f, b);
    {
        volatile float z0 = 0.0f;
        head[1] = z0 / z0;
    }
    Check(!AiOtsBox(head, lf, rf, lh, rh, c, &W, &H), "ots: a non-finite bone: no box (no fade this frame)");
    OtsBones(0, org, 0.0f, head, lf, rf, lh, rh);
    lf[1] = rf[1] = head[1] + 10.0f;
    Check(!AiOtsBox(head, lf, rf, lh, rh, c, &W, &H), "ots: a collapsed skeleton (head at the ankles): no box");
    // single copies: camera 220 u behind her (+z forward), 40 u to the right, at her chest height
    {
        const float eye[3] = { 40.0f, -230.0f, -220.0f }, her[3] = { 0.0f, -166.8f, 0.0f };
        float p[3];
        p[0] = 0.0f, p[1] = -166.8f, p[2] = -80.0f;  // on her path, 80 u behind her: between the camera and her, over her
        k = AiOtsK(eye, her, We, He, p, 0.3f);
        sprintf(b, "ots: a copy 80 u behind her on her path keeps %.2f (0.30 .. 0.40)", k);
        Check(k >= 0.3f - 1e-4f && k <= 0.40f, b);
        p[2] = 60.0f;  // ahead of her (farther from the camera): her body hides it, untouched
        Check(AiOtsK(eye, her, We, He, p, 0.3f) == 1.0f, "ots: a copy farther than her: untouched");
        p[2] = -260.0f;  // behind the camera
        Check(AiOtsK(eye, her, We, He, p, 0.3f) == 1.0f, "ots: a copy behind the lens: untouched");
        p[2] = -3.0f;  // at her depth (an about-turn's fold)
        Check(AiOtsK(eye, her, We, He, p, 0.3f) > 0.95f, "ots: a copy at her own depth: (nearly) untouched");
        p[2] = -80.0f;
        Check(AiOtsK(eye, her, We, He, p, 1.0f) == 1.0f && AiOtsK(eye, her, We, He, p, 0.0f) < 0.1f, "ots: minK 1 = no fade, minK 0 = dark over her");
        mono = 1, prev = 0.0f, bad = 0;
        for (i = 0; i <= 60; ++i) {  // sliding the copy sideways at 80 u behind her: from minK to full, never back down
            p[0] = 10.0f * (float)i;
            k = AiOtsK(eye, her, We, He, p, 0.3f);
            if (k < prev - 1e-5f) mono = 0;
            if (!(k >= 0.3f - 1e-4f && k <= 1.0f)) bad = 1;
            prev = k;
        }
        sprintf(b, "ots: a copy slid sideways brightens steadily to full (%.2f at 600 u)", prev);
        Check(mono && !bad && prev == 1.0f, b);
        p[0] = 90.0f, p[2] = -80.0f;  // a copy just outside Eileen's box on screen but inside Henry's
        {
            const float ke = AiOtsK(eye, her, We, He, p, 0.3f), kh = AiOtsK(eye, her, Wh, Hh, p, 0.3f);
            sprintf(b, "ots: the same copy beside her: Eileen %.2f, Henry (broader) %.2f", ke, kh);
            Check(kh < ke - 0.02f, b);
        }
    }
    // a running trail seen over her shoulder, both bodies, every yaw: the near copies over her fade, past the lens none
    for (body = 0; body < 2; ++body) {
        int y, faded = 0, kept = 0, wrong = 0, total = 0;
        for (y = 0; y < 8; ++y) {
            const float yaw = 0.785398f * (float)y, fx = sinf(yaw), fz = cosf(yaw), rx = cosf(yaw), rz = -sinf(yaw);
            float bh[3], blf[3], brf[3], blh[3], brh[3], bc[3], bw, bhh, eye[3];
            int n;
            OtsBones(body, org, yaw, bh, blf, brf, blh, brh);
            if (!AiOtsBox(bh, blf, brf, blh, brh, bc, &bw, &bhh)) { ++wrong; continue; }
            eye[0] = -fx * 220.0f + rx * 40.0f, eye[1] = -230.0f, eye[2] = -fz * 220.0f + rz * 40.0f;
            for (n = 1; n <= 9; ++n) {  // Long: 8 copies over 0.6 s at 400 u/s: 30 .. 240 u back along her path; + one past the lens
                const float back = n <= 8 ? 30.0f * (float)n : 270.0f;
                float at[3], ch[3], clf[3], crf[3], clh[3], crh[3], cc[3], cw, chh;
                at[0] = -fx * back, at[1] = 0.0f, at[2] = -fz * back;
                OtsBones(body, at, yaw, ch, clf, crf, clh, crh);
                if (!AiOtsBox(ch, clf, crf, clh, crh, cc, &cw, &chh)) { ++wrong; continue; }
                k = AiOtsK(eye, bc, bw, bhh, cc, 0.3f);
                ++total;
                if (back < 160.0f) faded += k < 0.6f, wrong += !(k < 0.6f);  // between the lens and her, over her
                else if (back > 250.0f) kept += k == 1.0f, wrong += k != 1.0f;  // past the lens (a copy right at it covers the screen: it fades)
            }
        }
        sprintf(b, "ots: %s's running trail over her shoulder, 8 yaws: %d copies between the lens and her fade, %d past the lens kept, %d wrong of %d",
                body ? "Henry" : "Eileen", faded, kept, wrong, total);
        Check(wrong == 0 && faded == 8 * 5 && kept == 8 * 1, b);
    }
    {   // camera.inc's default over-the-shoulder camera (Distance 800, Side 0, Height 330): the whole Long trail lies between
        // it and her: every copy over her dims (the nearest, inside her body's depth ramp, less)
        float eye[3], bh[3], blf[3], brf[3], blh[3], brh[3], bc[3], bw, bhh, kmax = 0.0f;
        int n, dim = 0;
        for (body = 0; body < 2; ++body) {
            OtsBones(body, org, 0.0f, bh, blf, brf, blh, brh);
            AiOtsBox(bh, blf, brf, blh, brh, bc, &bw, &bhh);
            eye[0] = 0.0f, eye[1] = -360.0f, eye[2] = -800.0f;
            for (n = 1; n <= 8; ++n) {
                float at[3], ch[3], clf[3], crf[3], clh[3], crh[3], cc[3], cw, chh;
                at[0] = 0.0f, at[1] = 0.0f, at[2] = -30.0f * (float)n;
                OtsBones(body, at, 0.0f, ch, clf, crf, clh, crh);
                AiOtsBox(ch, clf, crf, clh, crh, cc, &cw, &chh);
                k = AiOtsK(eye, bc, bw, bhh, cc, 0.3f);
                dim += k < 0.55f;
                if (n >= 2 && k > kmax) kmax = k;
            }
        }
        sprintf(b, "ots: the default camera (800 back, 330 up): %d of 16 copies (both bodies) dimmed under 0.55, copies 2..8 at most %.2f", dim, kmax);
        Check(dim >= 14 && kmax <= 0.31f, b);
    }
    {   // the same trail from a stock side camera 600 u to her right: no copy is in front of her, none fades
        float eye[3], bh[3], blf[3], brf[3], blh[3], brh[3], bc[3], bw, bhh;
        int n, dim = 0;
        OtsBones(0, org, 0.0f, bh, blf, brf, blh, brh);
        AiOtsBox(bh, blf, brf, blh, brh, bc, &bw, &bhh);
        eye[0] = 600.0f, eye[1] = -300.0f, eye[2] = 0.0f;
        for (n = 1; n <= 8; ++n) {
            float at[3], ch[3], clf[3], crf[3], clh[3], crh[3], cc[3], cw, chh;
            at[0] = 0.0f, at[1] = 0.0f, at[2] = -30.0f * (float)n;
            OtsBones(0, at, 0.0f, ch, clf, crf, clh, crh);
            AiOtsBox(ch, clf, crf, clh, crh, cc, &cw, &chh);
            dim += AiOtsK(eye, bc, bw, bhh, cc, 0.3f) < 0.98f;
        }
        sprintf(b, "ots: the trail seen from the side: %d of 8 copies would dim (the rule is not applied there anyway)", dim);
        Check(dim == 0, b);
    }
    {   // the weight: 0 -> 1 in 0.2 s at 60 and 180 Hz, back the same; a hitch or no time: no step
        static const int kHz[2] = { 60, 180 };
        int r;
        for (r = 0; r < 2; ++r) {
            float m = 0.0f, t = 0.0f;
            while (m < 1.0f && t < 1.0f) m = AiOtsMixStep(m, 1, 1.0f / (float)kHz[r]), t += 1.0f / (float)kHz[r];
            sprintf(b, "ots: %d Hz: the rule fades in over %.3f s", kHz[r], t);
            Check(fabsf(t - AI_OTS_TAU_S) <= 1.0f / (float)kHz[r] + 1e-4f, b);
            t = 0.0f;
            while (m > 0.0f && t < 1.0f) m = AiOtsMixStep(m, 0, 1.0f / (float)kHz[r]), t += 1.0f / (float)kHz[r];
            Check(fabsf(t - AI_OTS_TAU_S) <= 1.0f / (float)kHz[r] + 1e-4f, "ots: ... and out the same");
        }
        Check(AiOtsMixStep(0.4f, 1, 0.0f) == 0.4f && AiOtsMixStep(0.4f, 1, 0.7f) == 0.4f, "ots: no time / a hitch: the weight holds");
    }
    Check(AiOtsMinFromIni(-1) == AI_OTS_MIN_DEF && AiOtsMinFromIni(0) == 0.0f && AiOtsMinFromIni(45) == 0.45f && AiOtsMinFromIni(100) == 1.0f &&
          AiOtsMinFromIni(250) == 1.0f, "ots: AfterImageOtsFade: absent 30 %, 0 .. 100 %, over 100 = no fade");
}

int main(void)
{
    static const int kHz[6] = { 30, 60, 120, 144, 180, 240 };
    static RunOut r[6];
    char b[256];
    int i, tr, e;
    // ---- 1. the ring at six frame rates
    for (i = 0; i < 6; ++i) {
        Run(kHz[i], 3.0, &r[i]);
        sprintf(b, "%3d Hz: %d samples over 3 s (%.1f a second), the ring spans %.3f s, the widest gap %.4f s", kHz[i],
                r[i].samples, r[i].samples / 3.0, r[i].span, r[i].maxGap);
        Check(r[i].samples > 0 && r[i].span >= 1.0 && r[i].maxGap <= 1.0 / 30.0 + 1e-6 && r[i].samples <= 3 * 60 + 2, b);
    }
    for (tr = 0; tr < AI_TRAILS; ++tr)
        for (e = 0; e < kAiTrail[tr].n; ++e) {
            double worstExact = 0.0, worstSpread = 0.0, t = 3.0;
            int allOk = 1;
            for (i = 0; i < 6; ++i) {
                float ex[3];
                const double now = (double)(long)(3.0 * kHz[i] + 0.5) * (1.0 / (double)(float)kHz[i]);
                int j;
                allOk &= r[i].ok[tr][e];
                Path(now - AiLag(&kAiTrail[tr], e + 1), ex);
                for (j = 0; j < 3; j += 2) {
                    const double d = fabs(r[i].pos[tr][e][j] - ex[j]);
                    if (d > worstExact) worstExact = d;
                }
                if (i) {
                    const double s = fabs(r[i].pos[tr][e][2] - r[0].pos[tr][e][2]);
                    if (s > worstSpread) worstSpread = s;
                }
                (void)t;
            }
            sprintf(b, "trail %d echo %d (lag %.3f s): at every rate, error to the true path %.3f u, z spread over the rates %.3f u",
                    tr, e + 1, AiLag(&kAiTrail[tr], e + 1), worstExact, worstSpread);
            Check(allOk && worstExact < 1.0 && worstSpread < 1.5, b);  // (|f''| h^2 / 8 <= 0.5 u at h = 1/30 s)
        }
    {   // the trail grows out of her: 0.1 s after a reset only the echoes younger than that exist
        int k, broke, i0, i1, have = 0;
        float f, p[3];
        double now = 0.0;
        AiRingReset(&g_r);
        for (k = 0; k < 6; ++k) {  // 0.1 s at 60 Hz
            now += 1.0 / 60.0;
            Path(now, p);
            if (AiRingDue(&g_r, now)) AiRingPush(&g_r, now, p, &broke);
        }
        for (k = 1; k <= 5; ++k) have += AiRingAt(&g_r, now - AiLag(&kAiTrail[1], k), &i0, &i1, &f);
        Check(have == 1, "growing: 0.1 s after a reset only echo 1 (0.07 s) exists of the Medium trail");
        AiRingReset(&g_r);
        AiRingPush(&g_r, 1.0, p, &broke);
        Check(!AiRingAt(&g_r, 1.0, &i0, &i1, &f), "no echo from a single sample");
    }
    {   // breaks
        float p[3] = { 0, 0, 0 }, q[3] = { 310.0f, 0, 0 }, s[3] = { 250.0f, 0, 0 };
        int broke;
        AiRingReset(&g_r);
        AiRingPush(&g_r, 1.0, p, &broke);
        AiRingPush(&g_r, 1.0 + 1.0 / 60.0, s, &broke);
        Check(!broke && g_r.count == 2, "250 u in one sample (a dodge at 15 000 u/s would be faster than any move): kept");
        AiRingPush(&g_r, 1.0 + 2.0 / 60.0, q, &broke);
        Check(broke == 0 && g_r.count == 3, "60 u from the last sample: kept");
        AiRingPush(&g_r, 1.0 + 3.0 / 60.0, p, &broke);
        Check(broke && g_r.count == 1, "a 310 u jump (a door, a teleport): the ring starts again");
        AiRingPush(&g_r, 1.4, p, &broke);
        Check(broke && g_r.count == 1, "a 0.35 s gap: the ring starts again");
        AiRingPush(&g_r, 1.3, p, &broke);
        Check(broke && g_r.count == 1, "time going backwards: the ring starts again");
        Check(!AiRingDue(&g_r, 1.3 + 1.0 / 120.0) && AiRingDue(&g_r, 1.3 + 2.0 / 120.0), "due: not after 1/120 s, after 2 x 1/120 s");
    }
    // ---- 2. the look's factors
    {
        const float her[3] = { 0, 0, 0 }, a[3] = { 0, 0, 19.0f }, m[3] = { 30.0f, -50.0f, 30.0f }, z[3] = { 71.0f, 0, 0 };
        Check(AiMoveK(a, her) == 0.0f && AiMoveK(z, her) == 1.0f, "movement gate: 0 under 20 u, 1 from 70 u");
        Check(fabsf(AiMoveK(m, her) - AiSmooth(20.0f, 70.0f, sqrtf(1800.0f))) < 1e-6f, "movement gate: horizontal distance only");
        Check(AiFall(1, 5) == 1.0f && AiFall(5, 5) > 0.0f && AiFall(5, 5) < AiFall(4, 5), "the chain dims toward its tail");
        Check(AiLensK(300.0f, 300.0f) == 1.0f && AiLensK(500.0f, 300.0f) == 1.0f && AiLensK(205.0f, 300.0f) == 0.0f &&
              AiLensK(250.0f, 300.0f) > 0.3f && AiLensK(250.0f, 300.0f) < 0.7f, "lens gate: full beside / behind her, gone 90 u nearer the camera");
    }
    {
        double t;
        int n = 0, cracks = 0, bad = 0;
        float lo = 9.0f, hi = 0.0f;
        for (t = 0.0; t < 200.0; t += 0.05) {
            const float v = AiFlicker(t + 0.001, 2);
            if (v != AiFlicker(t + 0.001, 2)) ++bad;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
            cracks += v > 1.01f;
            ++n;
        }
        sprintf(b, "flicker: range %.3f .. %.3f, cracks in %.1f %% of the 50 ms windows (6 %% asked), deterministic", lo, hi, 100.0 * cracks / n);
        Check(!bad && lo >= 0.8f && hi <= 1.5f && cracks > n * 0.04 && cracks < n * 0.08, b);
    }
    // ---- 3. contact
    {
        const float a[3] = { 0.0f, -10.0f, 0.0f }, bb[3] = { 0.0f, -90.0f, 100.0f };  // a leg from the foot up to the hip
        float c[3] = { 60.0f, -50.0f, 50.0f }, out[3], d;
        Check(AiCapsuleHit(c, 50.0f, a, bb, AI_CAP_R, out, &d) && fabsf(d - 60.0f) < 0.01f && fabsf(out[2] - 50.0f) < 0.01f,
              "contact: 60 u beside the leg's middle with r 50 + 18: a hit at the leg's middle");
        c[0] = 70.0f;
        Check(!AiCapsuleHit(c, 50.0f, a, bb, AI_CAP_R, out, &d), "contact: 70 u beside it: no hit (50 + 18 = 68)");
        c[0] = 10.0f, c[1] = -300.0f;
        Check(!AiCapsuleHit(c, 50.0f, a, bb, AI_CAP_R, out, &d), "contact: 210 u above the span: no hit");
        c[1] = -230.0f;
        Check(AiCapsuleHit(c, 50.0f, a, bb, AI_CAP_R, out, &d), "contact: 140 u above the span's top: a hit (within 150)");
        c[0] = 0.0f, c[1] = -50.0f, c[2] = 140.0f;
        Check(AiCapsuleHit(c, 50.0f, a, bb, AI_CAP_R, out, &d) && fabsf(out[2] - 100.0f) < 0.01f && fabsf(d - 40.0f) < 0.01f,
              "contact: beyond the hip end: the end point is the contact");
        Check(AiTypeR(0x07) > 0.0f && AiTypeR(0x01) == 0.0f && AiTypeR(0x05) == 0.0f && AiTypeR(0x10) == 0.0f && AiTypeR(0x04) == 0.0f &&
              AiTypeR(0x12) == 0.0f && AiTypeR(0x11) > 0.0f, "types: never 1, 4, 5, 0x10, 0x12+");
    }
    // ---- 4. the cooldown, the same at every rate
    for (i = 0; i < 6; ++i) {
        static AiFoes T;
        const int hz = kHz[i];
        long k, n = 40L * hz;
        int stuns[2] = { 0, 0 }, sc;
        double firstGapUp[2] = { 1e9, 1e9 }, lastStun[2] = { -1, -1 }, upSince = 0.0;
        for (sc = 0; sc < 2; ++sc) {  // scenario 0: always standing and touched; 1: every stun knocks it down for 5 s
            double downUntil = -1.0, minUp = 1e9;
            AiFoe *f;
            AiFoesReset(&T);
            T.clock = 0.0;
            for (k = 0; k < n; ++k) {
                const double t = (double)k / hz;
                int down;
                T.clock = t;
                f = AiFoeFind(&T, (void *)0x1000, (void *)0x2000);
                down = sc == 1 && t < downUntil;
                if (!down && sc == 1 && t >= downUntil && downUntil > 0 && upSince < downUntil) upSince = downUntil;
                AiFoeStep(f, down, 1.0f / (float)hz);
                if (!down && AiFoeReady(f)) {
                    if (lastStun[sc] >= 0.0 && sc == 1 && t - upSince < minUp) minUp = t - upSince;
                    if (lastStun[sc] >= 0.0 && t - lastStun[sc] < firstGapUp[sc]) firstGapUp[sc] = t - lastStun[sc];
                    AiFoeStunned(f);
                    lastStun[sc] = t;
                    ++stuns[sc];
                    if (sc == 1) downUntil = t + 5.0;
                }
            }
            if (sc == 1) firstGapUp[1] = minUp;
        }
        sprintf(b, "%3d Hz: standing 40 s: %d stuns, %.3f s apart; knocked down 5 s each: %d stuns, %.3f s on its feet before each next",
                hz, stuns[0], firstGapUp[0], stuns[1], firstGapUp[1]);
        Check(stuns[0] == 14 && fabs(firstGapUp[0] - 3.0) <= 1.0 / hz + 1e-6 && stuns[1] == 6 &&
              firstGapUp[1] >= 2.0 - 1.0 / hz - 1e-4 && firstGapUp[1] <= 2.0 + 1.0 / hz + 1e-6, b);
    }
    {   // the table: reuse, gap, full
        static AiFoes T;
        AiFoe *f;
        int k;
        AiFoesReset(&T);
        T.clock = 1.0;
        f = AiFoeFind(&T, (void *)1, (void *)2);
        AiFoeStunned(f);
        T.clock = 1.1;
        f = AiFoeFind(&T, (void *)1, (void *)2);
        Check(f->stuns == 1 && !AiFoeReady(f), "table: the same task + work 0.1 s later is the same being (cooling down)");
        f = AiFoeFind(&T, (void *)1, (void *)3);
        Check(f->stuns == 0 && AiFoeReady(f), "table: the task with a new work is a new being (ready)");
        T.clock = 1.5;
        f = AiFoeFind(&T, (void *)1, (void *)3);
        Check(f->stuns == 0 && AiFoeReady(f), "table: missing 0.4 s = removed and re-made: a new being");
        for (k = 0; k < AI_FOES + 3; ++k) {
            T.clock = 2.0 + 0.001 * k;
            AiFoeFind(&T, (void *)(size_t)(100 + k), (void *)7);
        }
        Check(T.drops == 4, "table: full (1 + 51 beings in 48 slots): the stalest slot is taken (4 drops)");
        Check(AiFoeFind(&T, NULL, NULL) == NULL, "table: no task, no slot");
    }
    // ---- 5. v3.30 AS330: After Shock's lengths
    Check(kAiTrail[0].n == 3 && fabsf(kAiTrail[0].span - 0.20f) < 1e-6f && kAiTrail[1].n == 5 && fabsf(kAiTrail[1].span - 0.35f) < 1e-6f &&
          kAiTrail[2].n == 8 && fabsf(kAiTrail[2].span - 0.60f) < 1e-6f && AI_ECHO_MAX >= 8,
          "lengths: Short 3 over 0.2 s, Medium 5 over 0.35 s (v3.29's On), Long 8 over 0.6 s");
    Check(AiTrailIndex(0) == -1 && AiTrailIndex(1) == 0 && AiTrailIndex(2) == 1 && AiTrailIndex(3) == 2 && AiTrailIndex(4) == -1 &&
          AiTrailIndex(-1) == -1, "row values: 0 Off, 1 Short, 2 Medium, 3 Long, anything else off");
    Check(AI_RING * AI_SAMPLE_S >= kAiTrail[2].span + 0.25, "the ring (64 x 1/60 s) holds the Long trail with room to spare");
    Check(fabsf(AiLag(&kAiTrail[2], 8) - 0.6f) < 1e-6f && fabsf(AiLag(&kAiTrail[2], 1) - 0.075f) < 1e-6f &&
          fabsf(AiLag(&kAiTrail[0], 3) - 0.2f) < 1e-6f, "lags: Long 0.075 s apart up to 0.6 s, Short up to 0.2 s");
    {   // the stack fade: a Long trail 0.2 s into a side leap (850 u/s) from a standing start
        float last[3] = { 0, 0, 0 }, sumStack = 0.0f, sumPlain = 0.0f, firstAtStart = 0.0f;
        int k, have = 0;
        const float a0[3] = { 0, -94.0f, 0 }, a1[3] = { 1.5f, -94.0f, 0 }, a2[3] = { 10.0f, -94.0f, 0 };
        Check(AiStackK(a0, a0) == 0.0f && AiStackK(a1, a0) == 0.0f && AiStackK(a2, a0) == 1.0f, "stack fade: 0 within 2 u, full from 10 u (a walk: 12 u apart)");
        for (k = 1; k <= 8; ++k) {
            const double lag = AiLag(&kAiTrail[2], k), back = lag < 0.2 ? 0.2 - lag : 0.0;
            float p[3], w;
            p[0] = (float)(850.0 * back), p[1] = -94.0f, p[2] = 0.0f;
            w = AiFall(k, 8) * (have ? AiStackK(p, last) : 1.0f);
            sumPlain += p[0] == 0.0f ? AiFall(k, 8) : 0.0f;
            if (p[0] == 0.0f) {
                if (firstAtStart == 0.0f && w > 0.0f) firstAtStart = w;
                sumStack += w;
            }
            if (w > 0.004f) memcpy(last, p, sizeof last), have = 1;
        }
        sprintf(b, "stack fade: Long, a leap from standing: the copies where she stood add up to %.2f (v3.29 rule: %.2f), one echo's %.2f",
                sumStack, sumPlain, firstAtStart);
        Check(sumStack <= firstAtStart + 1e-5f && sumPlain > 1.5f * firstAtStart, b);
    }
    {   // the drawn-echo ladder and the spread of a capped trail
        int k, kk[8], ok = 1;
        Check(AiCapStep(8) == 5 && AiCapStep(5) == 3 && AiCapStep(3) == 3 && AiCapStep(6) == 5, "guard ladder: 8 -> 5 -> 3, floor 3");
        for (k = 1; k <= 5; ++k) kk[k - 1] = AiSpreadEcho(k, 5, 8);
        for (k = 1; k < 5; ++k) ok &= kk[k] > kk[k - 1];
        Check(ok && kk[0] >= 1 && kk[4] == 8, "a Long trail capped to 5: 5 distinct echoes spread over its length, the last at 0.6 s");
        Check(AiSpreadEcho(1, 3, 8) == 3 && AiSpreadEcho(2, 3, 8) == 6 && AiSpreadEcho(3, 3, 8) == 8 && AiSpreadEcho(2, 5, 5) == 2 &&
              AiSpreadEcho(3, 3, 3) == 3, "capped to 3: echoes 3, 6, 8; an uncapped trail keeps every echo");
    }
    {   // one contact per enemy and tick whatever the echo count; the stuns over 40 s the same for 3, 5 and 8 echoes
        static const unsigned char kCap[2][2] = { { 0, 1 }, { 2, 3 } };
        static float eb[AI_ECHO_MAX][AI_KEYS][3];
        static const int kRates[4] = { 30, 60, 120, 180 };
        int live[AI_ECHO_MAX], n, ok = 1, counts[3] = { 0, 0, 0 }, h;
        const float c[3] = { 0.0f, -90.0f, 0.0f };
        float at[3], best;
        for (n = 0; n < AI_ECHO_MAX; ++n) {  // every echo standing right on the enemy (a worst case: all 8 touch)
            int bb;
            for (bb = 0; bb < AI_KEYS; ++bb) eb[n][bb][0] = 5.0f * (float)n, eb[n][bb][1] = -10.0f - 10.0f * (float)bb, eb[n][bb][2] = 0.0f;
            live[n] = 1;
        }
        for (h = 0; h < 4; ++h) {
            static AiFoes T;
            const int hz = kRates[h];
            int tr;
            for (tr = 0; tr < AI_TRAILS; ++tr) {
                long k, steps = 40L * hz;
                int stuns = 0;
                AiFoesReset(&T);
                for (k = 0; k < steps; ++k) {
                    AiFoe *f;
                    T.clock = (double)k / hz;
                    f = AiFoeFind(&T, (void *)0x1000, (void *)0x2000);
                    AiFoeStep(f, 0, 1.0f / (float)hz);
                    if (AiBestContact(c, 50.0f, (const float(*)[AI_KEYS][3])eb, live, kAiTrail[tr].n, kCap, 2, 3, at, &best) < 0) ok = 0;
                    if (AiFoeReady(f)) AiFoeStunned(f), ++stuns;  // the caller: one stun at most per contact tick
                }
                counts[tr] = stuns;
            }
            sprintf(b, "%3d Hz: 40 s touched by every echo: Short %d, Medium %d, Long %d stuns (the cooldown's 14 each)", hz, counts[0],
                    counts[1], counts[2]);
            Check(ok && counts[0] == 14 && counts[1] == 14 && counts[2] == 14, b);
        }
        live[0] = live[1] = 0;  // the nearest echoes are not live (she stood): the contact comes from a live one or none
        n = AiBestContact(c, 50.0f, (const float(*)[AI_KEYS][3])eb, live, 8, kCap, 2, 3, at, &best);
        Check(n >= 2, "contact: only live echoes touch");
        memset(live, 0, sizeof live);
        Check(AiBestContact(c, 50.0f, (const float(*)[AI_KEYS][3])eb, live, 8, kCap, 2, 3, at, &best) == -1, "contact: no live echo, no contact");
    }
    Section6();
    Section7();
    Section8();  // v3.34 FX334
    Section9();  // v3.35 FX335
    Section10();  // v3.56 PW356
    printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
    return g_fail ? 1 : 0;
}
