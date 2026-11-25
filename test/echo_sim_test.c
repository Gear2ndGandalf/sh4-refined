// v3.30 ECHO: echo_sim.h native test (staging/special_mods/r4_echo/PLAN.md section 9): the Shadow delay rings at
// 30 / 60 / 120 / 144 Hz (the pose ring decimated to 60 Hz, the tick ring every tick), replay exactly once, pause and
// gap handling; the transforms (a shift, Shadow turn about her root); hit
// shapes moved with the double; Shadow's target choice and turn clamp; the window state machine (a swing's release
// tail, gun lines); one record per enemy per window, the record layout, and that the HP block's serial (+0x1C) is never
// written; the damage formula and the dropped ids. Build and run: test\build_echo_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/echo_sim.h"

static int g_fail, g_n, g_verbose;  // v3.30 ECHO330: -v prints every check
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

// ---- the Shadow pose ring: samples decimated to 60 Hz, looked up at clock - delay; the payload is linear in time, so
// the blend must give exactly the delayed time back (within float error)
static void RingDelay(int hz, float delay, int cap, float minStep, const char *name)
{
    EchoRing r;
    float x[ECHO_RING_MAX];
    float clock = 0.0f, worst = 0.0f;
    const float dt = 1.0f / (float)hz;
    int k, ok = 1, looks = 0;
    char buf[160];
    EchoRingInit(&r, cap);
    for (k = 0; k < hz * 4; ++k) {
        clock += dt;
        if (EchoRingDue(&r, clock, minStep)) x[EchoRingPush(&r, clock)] = clock;
        if (clock > delay + 0.5f) {
            int i0, i1;
            float a, got;
            const float want = clock - delay;
            if (!EchoRingFind(&r, want, &i0, &i1, &a)) {
                ok = 0;
                break;
            }
            got = x[i0] + (x[i1] - x[i0]) * a;
            if (fabsf(got - want) > worst) worst = fabsf(got - want);
            ++looks;
        }
    }
    sprintf_s(buf, sizeof buf, "%s at %d Hz: delay %.2f s within 1e-4 over %d look-ups (worst %.2e), oldest %.3f s back", name, hz, delay,
            looks, worst, clock - EchoRingOldest(&r));
    Check(ok && worst < 1e-4f && clock - EchoRingOldest(&r) >= delay, buf);
}

// ---- the tick ring's replay: every sample replayed once, in order, when it is `delay` old
static void Replay(int hz, float delay)
{
    EchoRing r;
    int seq[ECHO_RING_MAX], k, pushed = 0, replayed = 0, order = 1, lastSeq = -1;
    float clock = 0.0f, after = -1.0f, late = 0.0f;
    const float dt = 1.0f / (float)hz;
    char buf[128];
    EchoRingInit(&r, 160);
    for (k = 0; k < hz * 3; ++k) {
        int i;
        clock += dt;
        seq[EchoRingPush(&r, clock)] = pushed++;
        while ((i = EchoRingNext(&r, after, clock - delay)) >= 0) {
            if (seq[i] != lastSeq + 1) order = 0;
            lastSeq = seq[i];
            if (clock - r.t[i] - delay > late) late = clock - r.t[i] - delay;
            after = r.t[i];
            ++replayed;
        }
    }
    sprintf_s(buf, sizeof buf, "replay at %d Hz, delay %.2f: %d of %d replayed once, in order, at most %.4f s late", hz, delay, replayed, pushed, late);
    Check(order && abs(replayed - (pushed - (int)(delay * hz + 0.5f))) <= 1 && late <= dt + 1e-5f, buf);  // (float clock: +-1 at the edge)
}

// ---- v3.30 ECHO330: one shadow's delay schedule stepped at hz for 90 s (the game's clock += dt)
typedef struct { float worstSlope, lo, hi, worstVsDense; int rerolls, back; } DlyRun;
static DlyRun DlyAtRate(int hz, EcU32 seed)
{
    EchoDly d, ref;
    DlyRun o;
    float clock = 0.0f, prevD = -1.0f, prevTr = -1.0e9f, lastD1 = -1.0f;
    const float dt = 1.0f / (float)hz;
    int k;
    memset(&o, 0, sizeof o);
    o.lo = 1e9f, o.hi = -1e9f;
    EchoDlyInit(&d, seed, 0.0f, 0.3f);
    for (k = 0; k < hz * 90; ++k) {
        float v, tr;
        clock += dt;
        v = EchoDlyAt(&d, clock);
        {   // the same schedule evaluated fresh at this instant (no history): what any other rate sees at this time
            EchoDlyInit(&ref, seed, 0.0f, 0.3f);
            if (k % 97 == 0) {
                const float w = EchoDlyAt(&ref, clock);
                if (fabsf(w - v) > o.worstVsDense) o.worstVsDense = fabsf(w - v);
            }
        }
        if (v < o.lo) o.lo = v;
        if (v > o.hi) o.hi = v;
        if (prevD >= 0.0f && fabsf(v - prevD) / dt > o.worstSlope) o.worstSlope = fabsf(v - prevD) / dt;
        tr = clock - v;
        if (!(tr > prevTr)) ++o.back;
        if (d.d1 != lastD1) ++o.rerolls, lastD1 = d.d1;
        prevD = v, prevTr = tr;
    }
    return o;
}
// ---- the --look dump for tools/echo_preview.py: IN = one shadow per block:
//   "S t vis look flare floorY idx lite fx fz vx vy vz lr lg lb nEvt calm" (v3.32: + calm) then 16 bone lines "x y z", 3 head-axis lines
//   "x y z" (v3.31 ECHO331: rows 0-2 of the head bone, unit), then per event "E t0 kind seed" and 16 bone lines.
//   OUT: one sprite a line "kind add cell px py pz tx ty tz r a r g b push pad floor" (v3.32: + pad, floor).
static int LookDump(const char *inPath, const char *outPath)
{
    FILE *fi = NULL, *fo = NULL;
    static EchoSpr spr[ECHO_SPR_MAX * ECHO_MAXN];
    static EchoEvt ev[8];
    int total = 0;
    if (fopen_s(&fi, inPath, "r") || !fi) return 2;
    if (fopen_s(&fo, outPath, "w") || !fo) {
        fclose(fi);
        return 2;
    }
    for (;;) {
        EchoLookIn in;
        char tag[4];
        int nEvt, i, j, n;
        memset(&in, 0, sizeof in);
        if (fscanf_s(fi, "%3s", tag, (unsigned)sizeof tag) != 1 || tag[0] != 'S') break;
        if (fscanf_s(fi, "%f %f %d %f %f %d %d %f %f %f %f %f %f %f %f %d %f", &in.t, &in.vis, &in.look, &in.flare, &in.floorY, &in.idx,
                     &in.lite, &in.face[0], &in.face[1], &in.vel[0], &in.vel[1], &in.vel[2], &in.light[0], &in.light[1],
                     &in.light[2], &nEvt, &in.calm) != 17)  // v3.32 ECHO332: + calm
            break;
        for (i = 0; i < EB_N; ++i) fscanf_s(fi, "%f %f %f", &in.b[i][0], &in.b[i][1], &in.b[i][2]);
        for (i = 0; i < 3; ++i) fscanf_s(fi, "%f %f %f", &in.head[i][0], &in.head[i][1], &in.head[i][2]);
        if (nEvt > 8) nEvt = 8;
        for (j = 0; j < nEvt; ++j) {
            unsigned seed;
            fscanf_s(fi, "%3s %f %d %u", tag, (unsigned)sizeof tag, &ev[j].t0, &ev[j].kind, &seed);
            ev[j].seed = seed;
            for (i = 0; i < EB_N; ++i) fscanf_s(fi, "%f %f %f", &ev[j].b[i][0], &ev[j].b[i][1], &ev[j].b[i][2]);
        }
        n = EchoLookBuild(&in, ev, nEvt, spr, 0, ECHO_SPR_MAX);
        for (i = 0; i < n; ++i) {
            const EchoSpr *q = &spr[i];
            fprintf(fo, "%d %d %d %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.4f %.4f %.4f %.4f %.3f %d %.3f\n", q->kind, q->add, q->cell, q->p[0], q->p[1],
                    q->p[2], q->tip[0], q->tip[1], q->tip[2], q->r, q->a, q->rgb[0], q->rgb[1], q->rgb[2], q->push, q->pad & 1, q->floor);  // v3.32: + the floor fade bit, the floor
        }
        total += n;
    }
    fclose(fi);
    fclose(fo);
    printf("look: %d sprites written to %s\n", total, outPath);
    return 0;
}
static void LookIn(EchoLookIn *in, float t)  // a standing shadow at the origin, facing +z
{
    static const float kB[EB_N][3] = { { 0, -160, 0 }, { 0, -145, 0 }, { 0, -125, 0 }, { 0, -95, 0 }, { -18, -138, 0 }, { 18, -138, 0 },
                                       { -26, -110, 0 }, { 26, -110, 0 }, { -30, -85, 5 }, { 30, -85, 5 }, { -10, -90, 0 }, { 10, -90, 0 },
                                       { -11, -48, 2 }, { 11, -48, 2 }, { -12, -8, 0 }, { 12, -8, 0 } };
    memset(in, 0, sizeof *in);
    memcpy(in->b, kB, sizeof kB);
    in->face[1] = 1.0f;
    in->light[0] = 0.55f, in->light[1] = 0.20f, in->light[2] = 1.0f;
    in->t = t, in->vis = 1.0f, in->lite = 0;
    in->floorY = 0.0f;
    in->head[0][0] = in->head[1][1] = in->head[2][2] = 1.0f;  // v3.31: its head's axes (x side, y down, z forward)
}
// v3.31 ECHO331: the anchor heading at a tick rate: a script of her yaw / speed / hold, H sampled at given instants
typedef struct { float H[8]; } AncOut;
static float Deg(float r) { return r * 57.29578f; }
static AncOut AncRun(int hz, int script)
{
    EchoAnc a;
    AncOut o;
    const float dt = 1.0f / (float)hz;
    static const float kAt[8] = { 0.25f, 0.6f, 0.9f, 1.3f, 1.8f, 2.5f, 3.5f, 5.0f };  // (off the script's event ticks)
    float t = 0.0f, yaw = 0.0f;
    int k, s = 0;
    memset(&a, 0, sizeof a);
    memset(&o, 0, sizeof o);
    EchoAncStep(&a, 0.0f, 0.0f, 0.0f, dt, 0);
    for (k = 0; k < hz * 6 && s < 8; ++k) {
        float v = 0.0f;
        int hold = 0;
        t = (float)(k + 1) * dt;
        if (script == 0) {  // a chain spin: 2 turns a second for 1.5 s while she attacks, standing
            hold = t < 1.5f;
            yaw = t < 1.5f ? 12.566371f * t : yaw;
        } else if (script == 1) {  // she walks (200 u/s) and turns 90 degrees at t = 0.5
            v = 200.0f;
            yaw = t < 0.5f ? 0.0f : 1.5707964f;
        } else if (script == 2) {  // standing, an idle turn of 90 degrees at t = 0.2, held
            yaw = t < 0.2f ? 0.0f : 1.5707964f;
        } else {  // standing, an idle turn of 30 degrees (under the threshold)
            yaw = t < 0.2f ? 0.0f : 0.5235988f;
        }
        EchoAncStep(&a, EchoWrapPi(yaw), v * dt, 0.0f, dt, hold);
        if (t + 0.5f * dt >= kAt[s]) o.H[s++] = a.H;
    }
    return o;
}

// v3.33 ECHO333: v3.32's body-pass rule (echo.inc EchoBodies), kept to reproduce the user's "1 of 3": the budget's level 2
// dropped the prepass of the 2nd / 3rd shadow also while calm
static void EchoBodyPlan332(EchoBodyPl *p, int look, int level, int idx, float alpha, float k, int depthCfg)
{
    const int calmFade = (look == 0 || look == 2) && depthCfg != 0 && EchoCalmOp(k, ECHO_CALM_OPA) < 0.995f;
    memset(p, 0, sizeof *p);
    p->noWep = (level >= 1 && idx > 0) || level >= 3;
    p->lite = (level >= 2 && idx > 0) || level >= 3;
    if ((look == 0 || look == 2) && alpha >= 0.995f) {
        p->solid = 1, p->op = 1.0f;
        return;
    }
    p->depth = (depthCfg == 1 || (depthCfg == 2 && (look == 1 || look == 3)) || calmFade) && !(level >= 2 && idx > 0);
    p->op = p->depth ? alpha : 1.0f - powf(1.0f - alpha, ECHO_LAYERS);
}

int main(int argc, char **argv)
{
    static const int kHz[4] = { 30, 60, 120, 144 };
    int i;
    if (argc > 3 && !strcmp(argv[1], "--look")) return LookDump(argv[2], argv[3]);
    g_verbose = argc > 1 && !strcmp(argv[1], "-v");
    // 1. the rings
    for (i = 0; i < 4; ++i) {
        RingDelay(kHz[i], 0.35f, 48, 1.0f / 60.0f, "pose ring (48, 60 Hz decimation)");
        RingDelay(kHz[i], 0.60f, 48, 1.0f / 60.0f, "pose ring (48, 60 Hz decimation)");
        RingDelay(kHz[i], 0.60f, 160, 0.0f, "tick ring (160, every tick)");
        Replay(kHz[i], 0.35f);
        Replay(kHz[i], 0.15f);
    }
    RingDelay(240, 0.60f, 160, 0.0f, "tick ring (160, every tick)");
    {   // a pause: the clock stands, nothing is due, the look-up stays
        EchoRing r;
        int i0, i1;
        float a;
        EchoRingInit(&r, 48);
        EchoRingPush(&r, 1.0f), EchoRingPush(&r, 1.02f);
        Check(!EchoRingDue(&r, 1.02f, 1.0f / 60.0f), "pause: no sample due while the clock stands");
        Check(EchoRingFind(&r, 1.01f, &i0, &i1, &a) && Near(a, 0.5f, 1e-4f), "blend half way between two samples");
        EchoRingPush(&r, 1.6f);  // a 0.58 s gap (a hitch / a load): no blend across it
        Check(EchoRingFind(&r, 1.5f, &i0, &i1, &a) && i0 == i1 && Near(r.t[i0], 1.6f, 1e-6f), "gap: the nearer sample, no blend");
        Check(EchoRingFind(&r, 0.2f, &i0, &i1, &a) && Near(r.t[i0], 1.0f, 1e-6f), "before the oldest: the oldest");
        Check(EchoRingDue(&r, 0.5f, 0.0f) && r.count == 0, "the clock going back resets the ring");
    }
    // 2. transforms
    {
        const float root[3] = { 100.0f, -5.0f, 200.0f }, off[3] = { 120.0f, 0.0f, -30.0f };
        float M[16], p[3], f[2] = { 0.6f, 0.8f }, g[2], d[3], v[3] = { f[0], 0.0f, f[1] };
        EchoXformShadow(M, root, 0.7f, off);
        EchoPoint(M, root, p);
        Check(Near(p[0], 220.0f, 1e-3f) && Near(p[1], -5.0f, 1e-3f) && Near(p[2], 170.0f, 1e-3f), "Shadow: her root lands on root + offset");
        EchoVec(M, v, d);
        g[0] = d[0], g[1] = d[2];
        Check(Near(EchoAngleTo(f, g), 0.7f, 1e-4f), "Shadow: her facing turns by the own yaw (the header's convention)");
        EchoRot2(f, -1.1f, g);
        Check(Near(EchoAngleTo(f, g), -1.1f, 1e-4f), "EchoAngleTo inverts EchoRot2");
        Check(Near(EchoDet3(M), 1.0f, 1e-4f), "Shadow keeps the winding (det +1)");
        EchoXformShift(M, off);
        EchoPoint(M, root, p);
        Check(Near(p[0], 220.0f, 1e-4f) && Near(p[2], 170.0f, 1e-4f) && Near(EchoDet3(M), 1.0f, 1e-6f), "a shift (the spawn rise)");
    }
    // 3. shapes moved with the double
    {
        unsigned char s[ECHO_SHAPE_SZ];
        float *f = (float *)s, M[16], c[3];
        const float root[3] = { 0, 0, 0 }, off[3] = { 50, 0, 0 };
        memset(s, 0, sizeof s);
        s[0xA0] = 5;  // a box: identity axes, centre (10, -100, 40), half (5, 20, 60)
        EchoIdent(f);
        f[12] = 10, f[13] = -100, f[14] = 40;
        f[16] = 5, f[17] = 20, f[18] = 60;
        EchoXformShadow(M, root, 3.14159265f * 0.5f, off);
        EchoShapeXform(s, M);
        EchoShapeCentre(s, c);
        Check(Near(c[0], 50.0f - 40.0f, 1e-3f) && Near(c[1], -100.0f, 1e-3f) && Near(c[2], 10.0f, 1e-3f) && Near(f[16], 5.0f, 0) &&
                  Near(f[0], 0.0f, 1e-5f) && Near(f[2], 1.0f, 1e-5f),
              "box: centre turned about her root and shifted, axes turned, half extents kept");
        memset(s, 0, sizeof s);
        s[0xA0] = 1;  // a gun line
        f[0] = 0, f[1] = -150, f[2] = 0, f[4] = 0, f[5] = -150, f[6] = 2000;
        EchoXformShift(M, off);
        EchoShapeXform(s, M);
        Check(Near(f[0], 50, 0) && Near(f[4], 50, 0) && Near(f[6], 2000, 0), "line: both ends shifted");
        memset(s, 0, sizeof s);
        s[0xA0] = 3;  // the stomp's axis box
        f[0] = -20, f[1] = -10, f[2] = -30, f[4] = 20, f[5] = 10, f[6] = 30;
        EchoXformShadow(M, root, 0.5f, off);
        EchoShapeXform(s, M);
        Check(Near(f[4] - f[0], 40.0f, 1e-4f) && Near(f[6] - f[2], 60.0f, 1e-4f), "axis box: extents kept");
        s[0xA0] = 9;
        Check(!EchoShapeXform(s, M), "an unknown kind is refused");
    }
    // 4. Shadow's target and turn
    {
        EchoCand c[5] = { { { 0, 0, -200 }, 1 },    // behind
                          { { 0, 0, 700 }, 1 },     // too far
                          { { 50, 0, 150 }, 0 },    // blocked line of sight
                          { { -100, 0, 300 }, 1 },  // valid, 316
                          { { 200, 0, 100 }, 1 } }; // valid, 224: the nearest
        const float me[3] = { 0, 0, 0 }, fwd[2] = { 0, 1 };
        Check(EchoPickTarget(c, 5, me, fwd, 600.0f) == 4, "target: the nearest valid one in the front half within 600");
        c[4].ok = 0;
        Check(EchoPickTarget(c, 5, me, fwd, 600.0f) == 3, "target: the next one when the nearest is not valid");
        Check(EchoPickTarget(c, 3, me, fwd, 600.0f) == -1, "target: none");
        {
            const float side[2] = { 1, 0 };  // straight right: 90 degrees, clamped to 75
            const float a = EchoClampF(EchoAngleTo(fwd, side), -1.3089969f, 1.3089969f);
            float y = 0.0f;
            int k;
            Check(Near(fabsf(a), 1.3089969f, 1e-4f), "turn: clamped to 75 degrees from her facing");
            for (k = 0; k < 30; ++k) y = EchoTurnStep(y, a, 6.2831853f, 1.0f / 120.0f);  // 0.25 s at 360 deg/s = 90 deg
            Check(Near(y, a, 1e-4f), "turn: 360 deg/s reaches 75 degrees within 0.25 s");
            y = EchoTurnStep(0.0f, a, 6.2831853f, 1.0f / 30.0f);
            Check(Near(fabsf(y), 6.2831853f / 30.0f, 1e-4f), "turn: one 30 Hz step is 12 degrees");
        }
    }
    // 5. the window state machine
    {
        EchoWin w;
        const EcU32 S = 0xBA1E0042u;
        float t = 0.0f;
        int k, open = 0;
        memset(&w, 0, sizeof w);
        for (k = 0; k < 3; ++k, t += 1.0f / 30.0f) open += EchoWinStep(&w, S, 1, 1, 1, 0, t);  // 3 ticks held
        Check(open == 3, "swing: open while her attack carries S");
        for (k = 0, open = 0; k < 20; ++k, t += 1.0f / 30.0f) open += EchoWinStep(&w, 0, 0, 1, 1, 0, t);  // released after a hit
        Check(open >= 4 && open <= 7, "swing: a tail of held + 0.15 s after the release (4-7 ticks at 30 Hz)");
        memset(&w, 0, sizeof w);
        t = 0.0f;
        for (k = 0, open = 0; k < 10; ++k, t += 1.0f / 30.0f) open += EchoWinStep(&w, S, k < 2, 1, 1, 0, t);
        Check(open >= 2 && t - w.open > 0.0f, "swing: released early still gets a tail");
        Check(!EchoWinStep(&w, 0, 0, 0, 1, 0, t), "swing: no tail once she left the swing modes");
        memset(&w, 0, sizeof w);
        for (k = 0, open = 0; k < 6; ++k) open += EchoWinStep(&w, S + 1, 1, 0, 0, 1, (float)k / 60.0f);
        Check(open == ECHO_LINE_T, "gun line: tested on its first 2 ticks only");
        Check(EchoWinStep(&w, S + 2, 1, 0, 0, 1, 1.0f) && w.S == S + 2, "a new serial opens a new window");
    }
    // 6. one record per enemy per window; the record; the HP block's serial is never written
    {
        EchoFoeTab tab;
        EchoFoe *f;
        int a = 1, b = 2, wk1 = 10, wk2 = 11, hits = 0, k;
        unsigned char hp[0x30], rec[0x50];
        EcU32 counter = 0x1234FFFFu, s1, sentinel = 0xBA1E7777u, got;
        int id = 0x08, gotId;
        EchoHit h;
        EchoFoeReset(&tab);
        for (k = 0; k < 10; ++k) {  // ten ticks of one window on the same enemy
            tab.clock += 1.0f / 60.0f;
            hits += EchoFoeTake(EchoFoeFind(&tab, &a, &wk1), 0xBA1E0001u);
        }
        Check(hits == 1, "one record per enemy per window");
        Check(EchoFoeTake(EchoFoeFind(&tab, &b, &wk1), 0xBA1E0001u), "another enemy in the same window is hit too");
        Check(EchoFoeTake(EchoFoeFind(&tab, &a, &wk1), 0xBA1E0002u), "the next window hits the same enemy again");
        f = EchoFoeFind(&tab, &a, &wk2);  // the task slot reused for a new being (a new work block)
        Check(f && f->win == 0 && EchoFoeTake(f, 0xBA1E0002u), "a reused task slot is a new being (keyed by task + work)");
        tab.clock += 1.0f;
        f = EchoFoeFind(&tab, &b, &wk1);  // the same addresses after a gap: removed and made again
        Check(f && f->win == 0, "the same addresses after a gap are a new being");
        Check(!EchoFoeTake(f, 0), "window 0 never hits");
        s1 = EchoSerial(&counter);
        Check(s1 == 0xBA1EFFFFu && counter == 0x12350000u, "the twin serial is made as 0x4FE100 (counter advanced)");
        memset(hp, 0, sizeof hp);
        memcpy(hp + 0x1C, &sentinel, 4);  // her own serial S, stored by her own hit
        memset(&h, 0, sizeof h);
        h.pos[0] = 1, h.pos[3] = 1, h.dir[2] = 1, h.id = id, h.cat = 4, h.serial = s1, h.dmg = 12.5f, h.push = 20.0f, h.task = &a;
        EchoRecFill(rec, &h);
        EchoHpMark(hp, id);
        memcpy(&got, hp + 0x1C, 4);
        memcpy(&gotId, hp + 0x20, 4);
        Check(got == sentinel, "the HP block's serial (+0x1C) is untouched by a twin hit");
        Check(gotId == id, "the HP block's attack id (+0x20) is the twin's (stun holds, kill credit)");
        {
            EcU32 rs;
            float rd, rp;
            int rid, rcat;
            const void *rt, *ra;
            memcpy(&rid, rec + 0x20, 4), memcpy(&rcat, rec + 0x24, 4), memcpy(&rs, rec + 0x28, 4), memcpy(&rd, rec + 0x2C, 4);
            memcpy(&rp, rec + 0x30, 4), memcpy(&rt, rec + 0x40, 4), memcpy(&ra, rec + 0x44, 4);
            Check(rid == id && rcat == 4 && rs == s1 && rd == 12.5f && rp == 20.0f && rt == &a && ra == NULL &&
                      ((float *)rec)[3] == 1.0f && ((float *)rec)[6] == 1.0f,
                  "the record's fields (attack NULL)");
            Check(rs != sentinel, "the record carries the twin serial, not hers");
        }
    }
    // 7. damage and ids
    {
        int k, ok = 1;
        for (k = 0; k <= 10; ++k) {
            const float r = (float)k / 10.0f, d = EchoDamage(20.0f, 1.0f, r, 0.5f, 0x08), e = EchoDamage(20.0f, 1.2f, r, 0.75f, 0x08);
            ok &= d >= 20.0f * 0.85f * 0.5f - 1e-4f && d <= 20.0f * 1.15f * 0.5f + 1e-4f;
            ok &= e >= 20.0f * 1.2f * 0.8333f * 0.75f - 1e-3f && e <= 20.0f * 1.2f * 0.75f + 1e-3f;
        }
        Check(ok, "damage: hers x resist x 0.85-1.15 (0.833-1.0 above 1) x share");
        Check(EchoDamage(20.0f, 0.0f, 0.5f, 0.5f, 8) == 0.0f && EchoDamage(0.0f, 1.0f, 0.5f, 0.5f, 8) == 0.0f, "damage: none when resist or base is 0");
        Check(Near(EchoDamage(10.0f, 1.0f, 0.5f, 1.0f, 0x66), 20.0f, 1e-4f), "damage: ids 0x66-0x6C doubled (as 0x44F410)");
        Check(EchoIdDropped(0x49, 3) && EchoIdDropped(0x65, 3) && EchoIdDropped(0x6D, 3) && EchoIdDropped(0x2B, 6) &&
                  !EchoIdDropped(0x2B, 7) && !EchoIdDropped(0x08, 3) && !EchoIdDropped(0x2A, 7) && !EchoIdDropped(0x48, 3),
              "ids: the monster ids and the spray on the wheelchair are dropped; weapons, stun gun, SMG are kept");
    }
    // 8. the pose blend: a fast spin is not blended
    {
        float a[16], b[16], o[16];
        EchoIdent(a);
        EchoRotY(b, 0.2f);
        b[12] = 10;
        EchoMatBlend(o, a, b, 0.5f);
        Check(Near(o[12], 5.0f, 1e-5f) && Near(sqrtf(o[0] * o[0] + o[2] * o[2]), 1.0f, 1e-4f), "blend: rows keep their length");
        EchoRotY(b, 1.0f);
        EchoMatBlend(o, a, b, 0.4f);
        Check(!memcmp(o, a, sizeof o), "blend: over 30 degrees apart the nearer sample is used");
    }

    // ======== v3.30 ECHO330
    // 9. the ini's old values
    Check(EchoOldName("Shadow") && EchoOldName("twin") && EchoOldName("MIRROR") && !EchoOldName("Shadow Copy") && !EchoOldName("Off") &&
              !EchoOldName("Shadow Cast") && !EchoOldName(""),
          "ini: v3.29's Shadow / Twin / Mirror are recognised (read as Shadow Copy), the new names are not");
    // 10. each shadow's delay schedule
    {
        static const int kRate[4] = { 30, 60, 120, 180 };
        char buf[200];
        int r;
        for (r = 0; r < 4; ++r) {
            const DlyRun o = DlyAtRate(kRate[r], 0xC0FFEEu);
            sprintf_s(buf, sizeof buf, "delay at %d Hz: %.3f..%.3f s, slope at most %.3f s/s, %d values in 90 s, %d backward steps of "
                      "her replay, %.1e from the history-free schedule", kRate[r], o.lo, o.hi, o.worstSlope, o.rerolls, o.back, o.worstVsDense);
            Check(o.lo >= ECHO_DLY_MIN - 1e-5f && o.hi <= ECHO_DLY_MAX + 1e-5f && o.worstSlope <= ECHO_DLY_SLEW * 1.02f + 0.0005f * kRate[r] / 30.0f &&
                      o.rerolls >= 14 && o.rerolls <= 46 && o.back == 0 && o.worstVsDense < 2e-4f, buf);
        }
        {   // the same delay at the same instants whatever the tick rate (sampled at 30 Hz instants)
            EchoDly a, b;
            float ta = 0.0f, tb = 0.0f, worst = 0.0f;
            int k;
            EchoDlyInit(&a, 77u, 0.0f, 0.4f), EchoDlyInit(&b, 77u, 0.0f, 0.4f);
            for (k = 0; k < 30 * 60; ++k) {
                int j;
                float va, vb;
                ta = (float)(k + 1) / 30.0f;
                va = EchoDlyAt(&a, ta);
                for (j = 0; j < 6; ++j) tb = ((float)(k * 6 + j) + 1.0f) / 180.0f, vb = EchoDlyAt(&b, tb);
                if (fabsf(va - vb) > worst) worst = fabsf(va - vb);
            }
            sprintf_s(buf, sizeof buf, "delay: 30 Hz and 180 Hz runs agree at the shared instants (worst %.1e s)", worst);
            Check(worst < 1e-4f, buf);
        }
        {
            EchoDly a, b;
            EchoDlyInit(&a, 5u, 0.0f, 0.3f), EchoDlyInit(&b, 6u, 0.0f, 0.3f);
            Check(fabsf(EchoDlyAt(&a, 40.0f) - EchoDlyAt(&b, 40.0f)) > 1e-3f || fabsf(EchoDlyAt(&a, 47.0f) - EchoDlyAt(&b, 47.0f)) > 1e-3f,
                  "delay: two shadows (two seeds) do not move in step");
            Check(EchoDlyFirst(9u, 0, 3) < EchoDlyFirst(9u, 1, 3) && EchoDlyFirst(9u, 1, 3) < EchoDlyFirst(9u, 2, 3) &&
                      EchoDlyFirst(9u, 0, 3) >= ECHO_DLY_MIN && EchoDlyFirst(9u, 2, 3) <= ECHO_DLY_MAX,
                  "delay: three shadows start spread over 0.15-0.6 s");
            EchoDlyInit(&a, 5u, 10.0f, 0.3f);
            Check(Near(EchoDlyAt(&a, 10.0f), 0.3f, 1e-6f) && Near(EchoDlyAt(&a, 11.0f), 0.3f, 1e-6f), "delay: holds its first value until the first re-roll (2-6 s)");
        }
    }
    // 11. Shadow Cast's place, turn and teleport
    {
        EchoPl pl;
        float M[16], p[3], root[3] = { 500, -3, 900 };
        const float fwd[3] = { 0, 0, 1 };
        float v[3];
        memset(&pl, 0, sizeof pl);
        pl.away = 1, pl.yaw = 1.5707963f;
        memcpy(pl.anchor, root, sizeof root);
        pl.spot[0] = -200, pl.spot[1] = -40, pl.spot[2] = 50;
        EchoPlXform(M, root, &pl);
        EchoPoint(M, root, p);
        Check(Near(p[0], -200, 1e-3f) && Near(p[1], -40, 1e-3f) && Near(p[2], 50, 1e-3f), "cast: her replayed root at its anchor lands on the spot");
        EchoVec(M, fwd, v);
        Check(Near(v[0], -1.0f, 1e-4f) && Near(v[2], 0.0f, 1e-4f), "cast: her facing turned by its yaw (toward its enemy)");
        root[2] += 40.0f;  // she steps 40 forward in her swing: the shadow steps 40 along its own facing
        EchoPlXform(M, root, &pl);
        EchoPoint(M, root, p);
        Check(Near(p[0], -240, 1e-3f) && Near(p[2], 50, 1e-3f), "cast: her step in the swing is its step, turned");
        root[2] += 500.0f;  // she ran off: its drift stays capped
        EchoPlXform(M, root, &pl);
        EchoPoint(M, root, p);
        Check(Near(p[0], -200 - ECHO_DRIFT, 1e-2f) && Near(p[2], 50, 1e-2f), "cast: the drift is capped at 100 u");
        pl.away = 0, pl.off[0] = 120, pl.off[1] = 0, pl.off[2] = 0, pl.yaw = 0;
        EchoPlXform(M, root, &pl);
        EchoPoint(M, root, p);
        Check(Near(p[0], root[0] + 120, 1e-3f) && Near(p[2], root[2], 1e-3f), "home: beside her (the v3.29 form)");
        Check(Near(EchoTurnWrap(3.0f, -3.0f, 6.2831853f, 0.01f), EchoWrapPi(3.0f + 0.0628319f), 1e-4f), "turn: the short way round (through pi)");
    }
    {
        static const int kRate[4] = { 30, 60, 120, 180 };
        int r;
        for (r = 0; r < 4; ++r) {
            EchoTp tp;
            const float dt = 1.0f / (float)kRate[r];
            float t = 0.0f, tSwitch = -1.0f, tDone = -1.0f, worstJump = 0.0f, prev = 1.0f;
            int k, started;
            char buf[160];
            memset(&tp, 0, sizeof tp);
            started = EchoTpGo(&tp, 1);
            for (k = 0; k < kRate[r]; ++k) {
                const int ev = EchoTpStep(&tp, dt);
                const float vv = EchoTpVis(&tp);
                t += dt;
                if (ev == 1) tSwitch = t;
                if (ev == 2) tDone = t;
                if (ev != 1 && fabsf(vv - prev) > worstJump) worstJump = fabsf(vv - prev);
                prev = vv;
            }
            sprintf_s(buf, sizeof buf, "teleport at %d Hz: out %.3f s, formed at %.3f s, it ends at the enemy, the fade steps at most %.2f",
                      kRate[r], tSwitch, tDone, worstJump);
            Check(started && tp.ph == ETP_AWAY && fabsf(tSwitch - ECHO_TP_OUT) <= dt + 1e-4f &&
                      fabsf(tDone - ECHO_TP_OUT - ECHO_TP_IN) <= 2.0f * dt + 1e-4f && worstJump <= 0.5f * (kRate[r] <= 30 ? 1.0f : 0.6f), buf);
        }
        {
            EchoTp tp;
            float before;
            memset(&tp, 0, sizeof tp);
            EchoTpGo(&tp, 1);
            EchoTpStep(&tp, ECHO_TP_OUT);  // switched
            EchoTpStep(&tp, 0.5f * ECHO_TP_IN);
            before = EchoTpVis(&tp);
            Check(EchoTpGo(&tp, 0) && Near(EchoTpVis(&tp), before, 1e-4f) && tp.ph == ETP_OUT && tp.dest == 0,
                  "teleport: called back while half formed: it dissolves from the same visibility");
            Check(!EchoTpGo(&tp, 1) && tp.dest == 1, "teleport: a new destination while dissolving only changes where it goes");
        }
    }
    {
        int on[4] = { 0, 0, 0, 0 }, i, pick[3];
        for (i = 0; i < 3; ++i) pick[i] = EchoCastChoose(on, 1), ++on[pick[i]];
        Check(pick[0] == 0 && pick[1] == 0 && pick[2] == 0, "cast: one enemy: all three shadows on it");
        memset(on, 0, sizeof on);
        for (i = 0; i < 3; ++i) pick[i] = EchoCastChoose(on, 2), ++on[pick[i]];
        Check(pick[0] == 0 && pick[1] == 1 && pick[2] == 0, "cast: two enemies: 2 + 1 (the nearest gets the third)");
        memset(on, 0, sizeof on);
        for (i = 0; i < 3; ++i) pick[i] = EchoCastChoose(on, 4), ++on[pick[i]];
        Check(pick[0] == 0 && pick[1] == 1 && pick[2] == 2, "cast: many enemies: the three nearest, one each");
        Check(EchoCastChoose(on, 0) == -1, "cast: no enemy, no pick");
        {
            const float a0 = EchoSpotAngle(3u, 0, 1, 0), a1 = EchoSpotAngle(3u, 1, 1, 0), a2 = EchoSpotAngle(3u, 2, 1, 0);
            Check(Near(EchoWrapPi(a1 - a0), 2.0943951f, 1e-3f) && Near(EchoWrapPi(a2 - a1), 2.0943951f, 1e-3f) &&
                      !Near(EchoSpotAngle(3u, 0, 2, 0), a0, 1e-3f) && Near(EchoWrapPi(EchoSpotAngle(3u, 0, 1, 1) - a0), 0.7853982f, 1e-3f),
                  "cast: shadows on one enemy come in 120 degrees apart, a new plan from a new direction, retries 45 degrees on");
        }
        Check(Near(EchoFloorTol(170.0f), 85.0f, 1e-4f) && Near(EchoFloorTol(60.0f), 60.0f, 0) && Near(EchoFloorTol(400.0f), 110.0f, 0),
              "same floor: half her height, 60..110 u");
    }
    {
        unsigned char s[ECHO_SHAPE_SZ];
        float *f = (float *)s, c[3];
        const float tgt[3] = { 100, -120, 60 };
        memset(s, 0, sizeof s);
        s[0xA0] = 5;
        EchoIdent(f);
        f[12] = 20, f[13] = -110, f[14] = 0, f[16] = 10, f[17] = 30, f[18] = 50;
        Check(EchoMagnet(s, tgt, 130.0f), "reach: a swing box within reach moves");
        EchoShapeCentre(s, c);
        Check(Near(c[0], 100, 1e-3f) && Near(c[1], -120, 1e-3f) && Near(c[2], 60, 1e-3f) && Near(f[16], 10, 0), "reach: onto the enemy's hurt centre, extents kept");
        f[12] = 600;
        Check(!EchoMagnet(s, tgt, 130.0f), "reach: out of reach: not moved");
        s[0xA0] = 1;
        Check(!EchoMagnet(s, tgt, 1e9f), "reach: a gun line is never moved (it is aimed)");
    }
    Check(Near(EchoShareFor(0.5f, 1), 0.75f, 1e-6f) && Near(EchoShareFor(0.5f, 2), 0.60f, 1e-6f) && Near(EchoShareFor(0.5f, 3), 0.5f, 1e-5f) &&
              Near(EchoShareFor(1.0f, 1), 1.0f, 0),
          "shares: 75 % one shadow, 60 % each of two, 50 % each of three (at EchoDamage 50)");
    {
        EchoBud b;
        int k, down = 0;
        memset(&b, 0, sizeof b);
        for (k = 0; k < ECHO_SLOW_N - 1; ++k) down += EchoBudStep(&b, 4.0f);
        Check(!down && b.level == 0, "budget: 29 slow frames: no change");
        Check(EchoBudStep(&b, 4.0f) && b.level == 1, "budget: the 30th in a row: one level down");
        for (k = 0; k < ECHO_SLOW_N - 1; ++k) EchoBudStep(&b, 4.0f);
        EchoBudStep(&b, 1.0f);
        for (k = 0; k < 5; ++k) EchoBudStep(&b, 4.0f);
        Check(b.level == 1, "budget: a fast frame starts the count again");
        for (k = 0; k < 200; ++k) EchoBudStep(&b, 9.0f);
        Check(b.level == 3, "budget: at most level 3");
        memset(&b, 0, sizeof b);  // v3.31 ECHO331: a single slow frame
        Check(EchoBudStep(&b, 7.0f) == 2 && b.level == 1, "budget: one frame over 6 ms: one level down at once (v3.30 waited for 30)");
        for (k = 0, down = 0; k < ECHO_SPIKE_GAP - 1; ++k) down += EchoBudStep(&b, 1.0f + 8.0f * (float)(k == 5));
        Check(!down && b.level == 1, "budget: another spike within 20 frames: no second step (one hitch, one level)");
        Check(EchoBudStep(&b, 12.0f) == 2 && b.level == 2, "budget: a spike after the gap: the next level");
        for (k = 0; k < 100; ++k) EchoBudStep(&b, 2.5f);
        Check(b.level == 2, "budget: frames under 3 ms never step");
    }
    // 12. the sprite look (v3.32 ECHO332: the flame coat, wisps, softer eyes, calm)
    {
        static EchoSpr o[ECHO_SPR_MAX + 64], o2[ECHO_SPR_MAX + 64];
        EchoLookIn in;
        EchoEvt ev[2];
        int n, k, eyes = 0, ground = 0, black = 0, edges = 0, wisps = 0, glows = 0, ok = 1;
        LookIn(&in, 3.25f);
        n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
        for (k = 0; k < n; ++k) {
            const EchoSpr *q = &o[k];
            if (q->kind == ESK_ORB && q->p[1] < -160.0f && q->p[2] > 15.0f) ++eyes;
            if (q->kind == ESK_SOFT && q->add) ++glows;
            if (q->kind == ESK_GROUND) ++ground;
            if (q->kind == ESK_FLAME && !q->add && q->rgb[0] == 0.0f && q->rgb[2] == 0.0f) ++black;
            if (q->kind == ESK_FLAME && !q->add && q->pad && q->rgb[2] > 0.5f) ++edges;
            if (q->kind == ESK_WISP) ++wisps;
            ok &= _finite(q->p[0] + q->p[1] + q->p[2] + q->r + q->a) && q->a > 0.0f && q->a <= 1.0f;
        }
        if (eyes != 2 || glows != 4 || ground != 1 || black != ECHO_TG_N || edges != 4 || wisps < 8 || wisps > 2 * ECHO_WISP_N || !ok || n > 39)
            printf("  (eyes %d glows %d ground %d black %d edges %d wisps %d ok %d n %d)\n", eyes, glows, ground, black, edges, wisps, ok, n);
        Check(eyes == 2 && glows == 4 && ground == 1 && black == ECHO_TG_N && edges == 4 && wisps >= 8 && wisps <= 2 * ECHO_WISP_N && ok && n <= 39,
              "look: the flame coat (12 black tongues + 4 violet edges), purple wisps (2 streaks each), 2 eyes (orb, glow, haze), a ground shadow: <= 39 sprites, all finite");
        {   // v3.32: tight: the coat's visible black stands at most 12 u outside the stand-in body (its bones + their radii)
            // sideways, and its highest visible tip at most 30 u over the head's top (v3.31's column: ~58 u over it, its half
            // width 72 u); the fill (quad area) far under v3.31's three column layers
            float side = 0.0f, top = 1.0e9f, area = 0.0f, body = 0.0f, a31;
            for (k = 0; k < EB_N; ++k) body = fabsf(in.b[k][0]) + 7.0f > body ? fabsf(in.b[k][0]) + 7.0f : body;
            for (k = 0; k < n; ++k) {
                const EchoSpr *q = &o[k];
                float dl[3], len, vt, ext;
                int j;
                if (q->kind != ESK_FLAME || q->add || q->rgb[2] != 0.0f) continue;
                for (j = 0; j < 3; ++j) dl[j] = q->tip[j] - q->p[j];
                len = sqrtf(dl[0] * dl[0] + dl[1] * dl[1] + dl[2] * dl[2]);
                vt = q->p[1] + ECHO_TG_TOP * dl[1];
                if (vt < top) top = vt;
                ext = fabsf(q->p[0] + 0.3f * dl[0]) + ECHO_TG_VIS * q->r;  // (its widest part: low in the tongue)
                if (ext > side) side = ext;
                area += 2.0f * q->r * (len / 0.8f > 2.0f * q->r ? len / 0.8f : 2.0f * q->r);
            }
            a31 = 2.0f * 72.0f * 308.0f + 2.0f * 57.6f * 277.0f + 2.0f * 79.2f * 323.0f;  // v3.31: edge, black, heart at h = 160
            {
                char buf[220];
                sprintf_s(buf, sizeof buf, "look: tight: the black %.1f u outside the body sideways (<= 12), its top %.1f u over the head top (<= 30; v3.31 ~58); "
                          "black fill %.0f %% of v3.31's column", side - body, -(top - (-160.0f - 22.0f)), 100.0f * area / a31);
                Check(side - body <= 12.0f && -(top + 182.0f) <= 30.0f && area < 0.6f * a31, buf);
            }
        }
        {   // the wisps: thin purple tongues near the coat's edge, rising above where they were born
            int okw = 1, cnt = 0;
            for (k = 0; k < n; ++k) {
                const EchoSpr *q = &o[k];
                if (q->kind != ESK_WISP) continue;
                ++cnt;
                okw &= !q->add && q->r <= 3.2f && q->rgb[2] > q->rgb[0] && q->rgb[0] > q->rgb[1] && q->a <= ECHO_WISP_A + 1e-4f;
                okw &= q->tip[1] < q->p[1] && fabsf(q->p[0]) < 90.0f && q->p[1] > -260.0f && q->p[1] < 0.0f;
            }
            Check(okw && cnt > 0, "look: the wisps are thin (streaks <= 3.2 u wide), purple, faint (<= 0.6), rising, within the coat's reach");
        }
        {   // the eyes: a wider, softer alpha gradient (v3.31: one 10 u glow at 0.55): a 15 u glow under 0.4 and a 30 u haze
            float gr = 0.0f, ga = 1.0f, hr = 0.0f;
            for (k = 0; k < n; ++k)
                if (o[k].kind == ESK_SOFT && o[k].add) {
                    if (o[k].r > 20.0f) hr = o[k].r;
                    else gr = o[k].r, ga = o[k].a;
                }
            Check(gr >= 14.0f && ga < 0.4f && hr >= 28.0f, "eyes: a soft glow of 15 u under 0.4 and a faint 30 u haze (the wider gradient)");
        }
        {   // v3.32: calm: the coat at 30 % (the body 45 %, echo.inc), the eyes at 70 %, the same shapes; calm 0 = the solid look
            float b0 = 0.0f, b1 = 0.0f, e0 = 0.0f, e1 = 0.0f;
            int n1, same = 1;
            for (k = 0; k < n; ++k) {
                if (o[k].kind == ESK_FLAME && o[k].rgb[2] == 0.0f && !o[k].add) b0 += o[k].a;
                if (o[k].kind == ESK_ORB) e0 += o[k].a;
            }
            in.calm = 1.0f;
            n1 = EchoLookBuild(&in, NULL, 0, o2, 0, ECHO_SPR_MAX);
            for (k = 0; k < n1; ++k) {
                if (o2[k].kind == ESK_FLAME && o2[k].rgb[2] == 0.0f && !o2[k].add) b1 += o2[k].a;
                if (o2[k].kind == ESK_ORB) e1 += o2[k].a;
            }
            for (k = 0; k < 7 + ECHO_TG_N && k < n && k < n1; ++k) same &= !memcmp(o[k].p, o2[k].p, 12) && o[k].r == o2[k].r;  // (ground, eyes, coat)
            Check(n1 >= 7 + ECHO_TG_N && same && Near(b1, ECHO_CALM_COAT * b0, 1e-3f) && Near(e1, 0.70f * e0, 1e-3f),
                  "calm: the coat at 30 % (the body 45 %), the eyes at 70 %, the same shapes");
            in.calm = 0.0f;
        }
        {   // v3.31: the eyes follow the head: turn and nod the head's axes (the body's facing unchanged): the eyes turn with it
            float R[3][3], p0[2][3], p1[2][3], d0 = 0.0f, d1 = 0.0f;
            const float yw = 0.7f, pt = 0.35f;  // 40 degrees turned, 20 nodded
            int e2 = 0, j;
            for (k = 0; k < n; ++k) if (o[k].kind == ESK_ORB && e2 < 2) memcpy(p0[e2++], o[k].p, 12);
            R[0][0] = cosf(yw), R[0][1] = 0.0f, R[0][2] = -sinf(yw);  // turned about y, then nodded about its own x
            R[2][0] = sinf(yw) * cosf(pt), R[2][1] = sinf(pt), R[2][2] = cosf(yw) * cosf(pt);
            R[1][0] = R[2][1] * R[0][2] - R[2][2] * R[0][1], R[1][1] = R[2][2] * R[0][0] - R[2][0] * R[0][2], R[1][2] = R[2][0] * R[0][1] - R[2][1] * R[0][0];
            memcpy(in.head, R, sizeof R);
            n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
            for (k = 0, e2 = 0; k < n; ++k) if (o[k].kind == ESK_ORB && e2 < 2) memcpy(p1[e2++], o[k].p, 12);
            ok = e2 == 2;
            for (j = 0; j < 2 && ok; ++j) {  // each eye = head + R x its local offset (the same distance from the head bone)
                const float lx = j ? -ECHO_EYE_X : ECHO_EYE_X, want[3] = { R[0][0] * lx + R[1][0] * ECHO_EYE_Y + R[2][0] * ECHO_EYE_Z,
                                                                          -160.0f + R[0][1] * lx + R[1][1] * ECHO_EYE_Y + R[2][1] * ECHO_EYE_Z,
                                                                          R[0][2] * lx + R[1][2] * ECHO_EYE_Y + R[2][2] * ECHO_EYE_Z };
                for (k = 0; k < 3; ++k) ok &= Near(p1[j][k], want[k], 1e-3f), d0 += (p1[j][k] - p0[j][k]) * (p1[j][k] - p0[j][k]);
                d1 += sqrtf((p1[j][0]) * (p1[j][0]) + (p1[j][1] + 160.0f) * (p1[j][1] + 160.0f) + p1[j][2] * p1[j][2]) -
                      sqrtf(p0[j][0] * p0[j][0] + (p0[j][1] + 160.0f) * (p0[j][1] + 160.0f) + p0[j][2] * p0[j][2]);
            }
            Check(ok && d0 > 100.0f && fabsf(d1) < 1e-2f, "eyes: on the head bone's axes: a 40 degree turn and 20 degree nod of the head move them with it (same distance)");
            memset(in.head, 0, sizeof in.head);
            n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
            for (k = 0, e2 = 0; k < n; ++k) if (o[k].kind == ESK_ORB && o[k].p[2] > 15.0f) ++e2;
            Check(e2 == 2, "eyes: no head axes given: the facing frame (v3.30's place)");
            LookIn(&in, 3.25f);
            in.lite = 1;
            n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
            for (k = 0, black = edges = wisps = 0; k < n; ++k) {
                black += o[k].kind == ESK_FLAME && o[k].pad && o[k].rgb[2] == 0.0f;
                edges += o[k].kind == ESK_FLAME && o[k].pad && o[k].rgb[2] > 0.5f;
                wisps += o[k].kind == ESK_WISP;
            }
            Check(black == ECHO_TG_LITE && !edges && wisps <= 6 && n <= 17, "look: the light look (budget): 6 tongues, no edge layer, <= 3 wisps, no haze (<= 17 sprites)");
            in.lite = 0;
        }
        {   // v3.33 ECHO333: Thermal: the coat as heat (12 additive dark-red tongues), a hot point (yellow-white) in each,
            // white-hot eyes, heat wisps; no black, no ground; see-through: every alpha well under 1; <= 48 (v3.32: none)
            int hot = 0, tg = 0, dark = 0, grd = 0, w2 = 0, orb = 0;
            float amax = 0.0f;
            in.look = 1;
            n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
            for (k = 0, ok = 1; k < n; ++k) {
                const EchoSpr *q = &o[k];
                hot += q->kind == ESK_SOFT && q->add && q->rgb[1] > 0.7f && q->rgb[2] > 0.3f && q->r < 4.5f;
                tg += q->kind == ESK_FLAME && q->add && q->rgb[0] > 0.5f && q->rgb[2] < 0.1f;
                dark += !q->add, grd += q->kind == ESK_GROUND, w2 += q->kind == ESK_WISP && q->add, orb += q->kind == ESK_ORB && q->rgb[2] > 0.8f;
                if (q->kind != ESK_ORB && q->a > amax) amax = q->a;
                ok &= _finite(q->p[0] + q->p[1] + q->p[2] + q->r + q->a);
            }
            if (hot != ECHO_TG_N || tg != ECHO_TG_N || dark || grd || orb != 2 || n > ECHO_SPR_MAX)
                printf("  (Thermal: hot %d tongues %d dark %d ground %d wisps %d orbs %d n %d amax %.2f)\n", hot, tg, dark, grd, w2, orb, n, amax);
            Check(ok && hot == ECHO_TG_N && tg == ECHO_TG_N && !dark && !grd && orb == 2 && w2 > 0 && amax <= 0.76f && n <= ECHO_SPR_MAX,
                  "look: Thermal (v3.33): the coat as heat (12 additive red tongues) with a yellow-white hot point in each, white-hot eyes, heat wisps; "
                  "no black, no ground; every part <= 0.75");
        }
        {   // v3.34 FX334: Spirit Vision's dark specters (v3.33: all light, the coat as pale spectral flames: "too much like the
            // player"): a dark smoke coat (12 premultiplied near-black tongues), a faint cold rim behind each (premultiplied
            // cold light, wider, 8 u farther back: drawn first, a fringe outside the coat), dark smoke wisps, no ground; the
            // only additive light is the eyes; the rim brighter while striking; <= 48 sprites, also in the light look
            int c, smoke[2] = { 0, 0 }, rim[2] = { 0, 0 }, wsp[2] = { 0, 0 }, addOther[2] = { 0, 0 }, grd = 0, nmax = 0, eyes = 0, lite;
            float rimA[2] = { 0.0f, 0.0f }, smokeA = 0.0f, rimPush = 0.0f, coatPush = 0.0f, rimR = 0.0f, coatR = 0.0f;  // (sums)
            char buf[400];
            LookIn(&in, 3.25f);
            in.look = 3;
            for (c = 0; c < 2; ++c) {
                in.calm = c ? 1.0f : 0.0f;
                n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
                if (n > nmax) nmax = n;
                for (k = 0; k < n; ++k) {
                    const EchoSpr *q = &o[k];
                    const float mx = q->rgb[0] > q->rgb[1] ? (q->rgb[0] > q->rgb[2] ? q->rgb[0] : q->rgb[2]) : (q->rgb[1] > q->rgb[2] ? q->rgb[1] : q->rgb[2]);
                    grd += q->kind == ESK_GROUND;
                    if (q->kind == ESK_FLAME && !q->add && mx <= 0.05f) {
                        ++smoke[c];
                        if (q->a > smokeA) smokeA = q->a;
                        coatPush += q->push, coatR += q->r;
                    } else if (q->kind == ESK_FLAME && !q->add && q->rgb[2] > 0.8f && q->rgb[2] > q->rgb[0]) {
                        ++rim[c], rimA[c] += q->a;
                        rimPush += q->push, rimR += q->r;
                    } else if (q->kind == ESK_WISP) {
                        wsp[c] += !q->add && mx <= 0.05f;
                        addOther[c] += q->add;
                    } else if (q->add) {
                        eyes += c == 0 && (q->kind == ESK_ORB || q->kind == ESK_SOFT);
                        addOther[c] += q->kind != ESK_ORB && q->kind != ESK_SOFT;
                    }
                }
            }
            in.calm = 0.0f, in.lite = 1;
            lite = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
            for (k = 0, c = 0; k < lite; ++k) c += o[k].kind == ESK_FLAME && !o[k].add && o[k].rgb[2] > 0.8f;
            in.lite = 0;
            sprintf_s(buf, sizeof buf, "look: Spirit Vision (v3.34 dark specters): %d / %d dark smoke tongues (alpha <= %.2f), %d / %d cold rim "
                      "tongues (striking %.2f vs calm %.2f summed), %d / %d dark wisps, eyes %d additive, no other light (%d / %d), no ground; "
                      "%d sprites at most (light look: %d, %d rims)", smoke[0], smoke[1], smokeA, rim[0], rim[1], rimA[0], rimA[1], wsp[0], wsp[1],
                      eyes, addOther[0], addOther[1], nmax, lite, c);
            Check(smoke[0] == ECHO_TG_N && smoke[1] == ECHO_TG_N && rim[0] == ECHO_TG_N && rim[1] == ECHO_TG_N && smokeA <= ECHO_SV_SMOKE + 1e-4f &&
                      rimA[0] > 1.8f * rimA[1] && rimA[1] > 0.0f && Near(rimPush, coatPush + 2.0f * ECHO_TG_N * 8.0f, 0.01f) &&
                      Near(rimR, 1.2f * coatR, 0.01f * coatR) && wsp[0] > 0 &&
                      eyes == 6 && !addOther[0] && !addOther[1] && !grd && nmax <= ECHO_SPR_MAX && c == ECHO_TG_LITE && lite <= 24, buf);
        }
        {   // v3.34 FX334: the body: Spirit Vision 62 % striking .. 45 % calm (dark, still see-through: under EchoOpCheck's 70 %),
            // Thermal / night vision unchanged (50 .. 35 %)
            char buf[240];
            sprintf_s(buf, sizeof buf, "body: Spirit Vision %.0f %% striking .. %.0f %% calm (dark specters); Thermal %.0f .. %.0f %%, night vision "
                      "%.0f .. %.0f %% (unchanged)", 100.0f * EchoBodyOp(3, 0.0f), 100.0f * EchoBodyOp(3, 1.0f), 100.0f * EchoBodyOp(1, 0.0f),
                      100.0f * EchoBodyOp(1, 1.0f), 100.0f * EchoBodyOp(2, 0.0f), 100.0f * EchoBodyOp(2, 1.0f));
            Check(Near(EchoBodyOp(3, 0.0f), 0.62f, 1e-5f) && Near(EchoBodyOp(3, 1.0f), 0.45f, 1e-5f) && EchoBodyOp(3, 0.0f) < 0.70f &&
                      Near(EchoBodyOp(1, 0.0f), 0.50f, 1e-5f) && Near(EchoBodyOp(1, 1.0f), 0.35f, 1e-5f) && Near(EchoBodyOp(2, 0.0f), 0.50f, 1e-5f) &&
                      Near(EchoBodyOp(2, 1.0f), 0.35f, 1e-5f), buf);
        }
        {   // v3.34 FX334: a teleport under Spirit Vision: dark smoke puffs with a cold crest on every other one (v3.33: pale light)
            EchoEvt ev2[2];
            int dk = 0, cr = 0, bad = 0;
            char buf[200];
            memset(ev2, 0, sizeof ev2);
            ev2[0].t0 = 3.0f, ev2[0].kind = 0, ev2[0].seed = 21u;
            ev2[1].t0 = 3.05f, ev2[1].kind = 1, ev2[1].seed = 22u;
            memcpy(ev2[0].b, in.b, sizeof in.b), memcpy(ev2[1].b, in.b, sizeof in.b);
            n = EchoLookBuild(&in, ev2, 2, o, 0, ECHO_SPR_MAX);
            for (k = 0; k < n; ++k) {
                dk += (o[k].kind == ESK_SMOKE0 || o[k].kind == ESK_SMOKE1) && !o[k].add && o[k].rgb[2] <= 0.05f;
                cr += o[k].kind == ESK_CREST && o[k].add;
                bad += o[k].kind == ESK_SOFT && o[k].add && o[k].r > 2.0f * ECHO_EYE_HAZE;
            }
            sprintf_s(buf, sizeof buf, "look: Spirit Vision teleport: %d dark smoke puffs, %d cold crests, %d pale puffs (v3.33's); %d sprites", dk, cr, bad, n);
            Check(dk > 0 && cr > 0 && !bad && n <= ECHO_SPR_MAX, buf);
        }
        {   // v3.34 FX334: Spirit Vision's list is a function of game time: the same at 30 / 60 / 120 / 180 Hz (7 / 3 s reached in
            // frame steps; a whole number of frames at each rate)
            static const int kHz4[4] = { 30, 60, 120, 180 };
            static EchoSpr oa[ECHO_SPR_MAX], ob[ECHO_SPR_MAX];
            int h, same = 1, na;
            LookIn(&in, 7.0f / 3.0f);
            in.look = 3, in.calm = 0.4f;
            na = EchoLookBuild(&in, NULL, 0, oa, 0, ECHO_SPR_MAX);
            for (h = 0; h < 4; ++h) {
                double tt = 0.0;
                int f2, nb;
                for (f2 = 0; f2 < kHz4[h] * 7 / 3; ++f2) tt += 1.0 / (double)kHz4[h];
                in.t = (float)tt;
                nb = EchoLookBuild(&in, NULL, 0, ob, 0, ECHO_SPR_MAX);
                same &= nb == na;
                for (k = 0; k < na && k < nb; ++k)
                    same &= oa[k].kind == ob[k].kind && Near(oa[k].a, ob[k].a, 2e-3f) && Near(oa[k].p[1], ob[k].p[1], 0.05f) && Near(oa[k].tip[0], ob[k].tip[0], 0.05f);
            }
            Check(same, "look: Spirit Vision's specters are the same at 30 / 60 / 120 / 180 Hz at the same game time");
            LookIn(&in, 3.25f);
        }
        {   // v3.33 ECHO333: night vision: the black coat see-through (<= 0.5 x 0.92), the edges and wisps additive pale
            // (phosphor after the filter), eyes as light; striking and calm alike
            int edg = 0, wsp = 0, blk = 0, okn = 1, c;
            LookIn(&in, 3.25f);
            in.look = 2;
            for (c = 0; c < 2; ++c) {
                in.calm = c ? 1.0f : 0.0f;
                n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
                for (k = 0; k < n; ++k) {
                    const EchoSpr *q = &o[k];
                    if (q->kind == ESK_FLAME && !q->add) ++blk, okn &= q->a <= ECHO_TG_BLACK * ECHO_NV_COAT + 1e-4f;
                    if (q->kind == ESK_FLAME && q->add) ++edg, okn &= q->rgb[1] > 0.8f;
                    if (q->kind == ESK_WISP) ++wsp, okn &= q->add && q->rgb[1] > 0.8f;
                }
            }
            Check(okn && blk == 2 * ECHO_TG_N && edg == 8 && wsp > 0, "look: night vision (v3.33): the black coat see-through (<= 46 %), a pale additive edge "
                  "and pale filaments (phosphor after the filter), striking and calm");
            LookIn(&in, 3.25f);
        }
        LookIn(&in, 3.25f);
        n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
        Check(EchoLookBuild(&in, NULL, 0, o2, 0, ECHO_SPR_MAX) == n && !memcmp(o, o2, sizeof(EchoSpr) * (size_t)n),
              "look: a function of the time alone (the same list at any frame rate)");
        {   // the wisps over 4 s at 30 / 60 / 120 / 180 Hz: the same list at the same instant whatever the steps before it
            float t;
            int same = 1, born = 0, prevW = -1;
            for (t = 0.0f; t < 4.0f; t += 1.0f / 180.0f) {
                int nw = 0;
                in.t = t;
                n = EchoLookBuild(&in, NULL, 0, o, 0, ECHO_SPR_MAX);
                for (k = 0; k < n; ++k) nw += o[k].kind == ESK_WISP;
                if (prevW >= 0 && nw != prevW) ++born;
                prevW = nw;
                same &= nw <= 2 * ECHO_WISP_N;
            }
            Check(same && born > 4, "look: the wisps come and go on their own lives (never more than 8 at once)");
            in.t = 3.25f;
        }
        memset(ev, 0, sizeof ev);
        ev[0].t0 = 3.0f, ev[0].kind = 0, ev[0].seed = 11u;
        ev[1].t0 = 3.0f, ev[1].kind = 1, ev[1].seed = 12u;
        memcpy(ev[0].b, in.b, sizeof in.b), memcpy(ev[1].b, in.b, sizeof in.b);
        {
            float p[3], r, a, worst = 0.0f;
            int j, gone = 1, zero = 1;
            for (j = 0; j < ECHO_PUFF_N; ++j) {
                gone &= !EchoPuffAt(&ev[0], j, ECHO_PUFF_OUT + 0.01f, p, &r, &a) && !EchoPuffAt(&ev[1], j, ECHO_PUFF_IN + 0.01f, p, &r, &a);
                if (EchoPuffAt(&ev[0], j, 0.0f, p, &r, &a)) zero &= a < 1e-4f;
            }
            {   // smoke in: every puff has gathered onto its bone (only the slow rise left) when the shadow has formed
                float q[3];
                EchoEvt e1 = ev[1];
                for (j = 0; j < ECHO_PUFF_N; ++j) {
                    int bi;
                    float best = 1e9f;
                    EchoPuffAt(&e1, j, ECHO_TP_IN, q, &r, &a);
                    for (bi = 0; bi < EB_N; ++bi) {
                        const float dx = q[0] - e1.b[bi][0], dy = q[1] + 20.0f * ECHO_TP_IN - e1.b[bi][1], dz = q[2] - e1.b[bi][2];
                        const float d = sqrtf(dx * dx + dy * dy + dz * dz);
                        if (d < best) best = d;
                    }
                    if (best > worst) worst = best;
                }
            }
            Check(gone && zero && worst < 1e-3f, "smoke: out puffs start unseen and are gone after 0.6 s; in puffs gather onto the body as it forms");
        }
        in.vis = 0.0f;
        n = EchoLookBuild(&in, ev, 2, o, 0, ECHO_SPR_MAX);
        for (k = 0, ok = n > 0; k < n; ++k) ok &= o[k].kind == ESK_SMOKE0 || o[k].kind == ESK_SMOKE1 || o[k].kind == ESK_CREST;
        Check(ok, "smoke: drawn while the shadow itself is unseen (only the smoke)");
        in.vis = 1.0f;
        {   // the busiest instant of two smoke events with the shadow fully shown (stricter than a real teleport) fits the
            // per-shadow cap without dropping anything: the decoration waits while the smoke is thick
            int mx = 0, smx = 0;
            float ta;
            int lost = 0;
            for (ta = 3.0f; ta < 3.8f; ta += 0.005f) {
                int sm = 0, rest = 0, alone;
                in.t = ta;
                n = EchoLookBuild(&in, ev, 2, o, 0, ECHO_SPR_MAX + 64);
                for (k = 0; k < n; ++k) {
                    const int isSm = o[k].kind == ESK_SMOKE0 || o[k].kind == ESK_SMOKE1 || o[k].kind == ESK_CREST;
                    sm += isSm, rest += !isSm;
                }
                in.vis = 0.0f;  // the same smoke alone (nothing else competes for the cap): what was wanted
                alone = EchoLookBuild(&in, ev, 2, o2, 0, ECHO_SPR_MAX + 64);
                in.vis = 1.0f;
                if (rest + alone > ECHO_SPR_MAX || sm < alone) ++lost;
                if (rest + alone > mx) mx = rest + alone;
                if (alone > smx) smx = alone;
            }
            in.t = 3.25f;
            {
                char buf[200];
                sprintf_s(buf, sizeof buf, "look: a shadow with two smoke events fits ECHO_SPR_MAX with nothing dropped (%d of 48 wanted at the busiest instant, "
                          "%d smoke; %d instants over)", mx, smx, lost);
                Check(mx <= ECHO_SPR_MAX && !lost, buf);
            }
        }
    }
    // 12b. v3.32 ECHO332: calm vs striking at 30 / 60 / 120 / 180 Hz, and the busy pose gate
    {
        static const int kR[4] = { 30, 60, 120, 180 };
        static const float kAt[7] = { 0.5f, 1.05f, 1.2f, 1.6f, 1.9f, 2.2f, 3.0f };
        float got[4][7];
        int j, s, ok = 1;
        char buf[200];
        for (j = 0; j < 4; ++j) {  // calm until 1.0 s, a strike from 1.0 to 1.5 s, then calm again
            EchoCalm c;
            const float dt = 1.0f / (float)kR[j];
            int k;
            memset(&c, 0, sizeof c);
            EchoCalmStep(&c, 0, dt);
            for (k = 1, s = 0; k <= kR[j] * 3 && s < 7; ++k) {
                const float t = (float)k * dt;
                EchoCalmStep(&c, t > 1.0f + 1e-4f && t <= 1.5f + 1e-4f, dt);
                if (t + 0.5f * dt >= kAt[s]) got[j][s++] = c.k;
            }
        }
        // expected: 1 before; solid 0.12 s after the strike starts; held 0.35 s after it ends (1.5 .. 1.85), then 0.5 s back
        ok &= Near(got[0][0], 1.0f, 1e-5f) && Near(got[0][2], 0.0f, 1e-5f) && Near(got[0][3], 0.0f, 1e-5f);
        ok &= Near(got[0][4], 0.1f, 0.07f) && Near(got[0][5], 0.7f, 0.07f) && Near(got[0][6], 1.0f, 1e-5f);
        for (j = 1; j < 4; ++j)
            for (s = 0; s < 7; ++s) ok &= Near(got[j][s], got[0][s], 1.0f / 30.0f / ECHO_CALM_IN + 1e-3f);
        sprintf_s(buf, sizeof buf, "calm: solid 0.12 s into a strike, held 0.35 s after it, calm 0.5 s later; the same path at 30 / 60 / 120 / 180 Hz "
                  "(at 1.9 s: %.3f / %.3f / %.3f / %.3f)", got[0][4], got[1][4], got[2][4], got[3][4]);
        Check(ok, buf);
        Check(Near(EchoCalmDist(1.0f), 1.5f, 1e-6f) && Near(EchoCalmDist(0.0f), 1.0f, 1e-6f) && EchoCalmDist(0.5f) > 1.0f && EchoCalmDist(0.5f) < 1.5f &&
                  Near(EchoCalmOp(1.0f, ECHO_CALM_OPA), 0.45f, 1e-6f) && Near(EchoCalmOp(0.0f, ECHO_CALM_OPA), 1.0f, 1e-6f),
              "calm: 1.5 x the slot distance and 45 % opacity when calm, the v3.31 distance and solid when striking");
        {   // eased: no step larger than one tick's share of the ramp's steepest slope (smoothstep x 1.5) at 30 Hz
            EchoCalm c;
            float prev, worst = 0.0f;
            int k;
            memset(&c, 0, sizeof c);
            EchoCalmStep(&c, 0, 1.0f / 30.0f);
            prev = EchoCalmOp(c.k, ECHO_CALM_OPA);
            for (k = 1; k < 90; ++k) {
                float v;
                EchoCalmStep(&c, k >= 30 && k < 45, 1.0f / 30.0f);
                v = EchoCalmOp(c.k, ECHO_CALM_OPA);
                if (fabsf(v - prev) > worst) worst = fabsf(v - prev);
                prev = v;
            }
            sprintf_s(buf, sizeof buf, "calm: the opacity eases (largest step %.3f at 30 Hz, <= 0.55 x 1.5 / 3.6 ticks)", worst);
            Check(worst <= 0.55f * 1.5f / (ECHO_CALM_IN * 30.0f) + 1e-3f, buf);
        }
        Check(EchoLiveOk(9.0f, 1, 10.0f, -1.0f, 0.05f) && !EchoLiveOk(9.97f, 1, 10.0f, -1.0f, 0.05f) && !EchoLiveOk(10.5f, 1, 10.0f, -1.0f, 0.05f),
              "busy gate: during a pickup only her pose from before it may show");
        Check(!EchoLiveOk(12.0f, 0, 10.0f, 12.2f, 0.05f) && !EchoLiveOk(12.24f, 0, 10.0f, 12.2f, 0.05f) && EchoLiveOk(12.3f, 0, 10.0f, 12.2f, 0.05f) &&
                  EchoLiveOk(9.0f, 0, 10.0f, 12.2f, 0.05f) && EchoLiveOk(5.0f, 0, 0.0f, -1.0f, 0.05f),
              "busy gate: after it ends, her pose shows again only once the shadow's replay time has passed its end (the 0.15-0.6 s delay)");
    }
    // 13. v3.31 ECHO331: the anchor heading at 30 / 60 / 120 / 180 Hz
    {
        static const int kR[4] = { 30, 60, 120, 180 };
        AncOut r[4][4];
        int s, j, k, ok;
        char buf[200];
        for (j = 0; j < 4; ++j)
            for (s = 0; s < 4; ++s) r[j][s] = AncRun(kR[j], s);
        for (j = 0, ok = 1; j < 4; ++j)
            for (k = 0; k < 8; ++k) ok &= fabsf(r[j][0].H[k]) < 1e-5f;
        Check(ok, "anchor: a chain spin (2 turns a second for 1.5 s while she attacks): the heading holds at every rate (no orbit)");
        for (j = 0, ok = 1; j < 4; ++j) ok &= fabsf(Deg(r[j][1].H[0])) < 1e-3f && fabsf(Deg(EchoWrapPi(r[j][1].H[5] - 1.5707964f))) < 2.0f;
        sprintf_s(buf, sizeof buf, "anchor: walking, a 90 degree turn: followed (%.1f deg off after 2 s at 30 Hz, %.1f at 180 Hz)",
                  Deg(1.5707964f - r[0][1].H[5]), Deg(1.5707964f - r[3][1].H[5]));
        Check(ok, buf);
        for (j = 1, ok = 1; j < 4; ++j)
            for (k = 0; k < 8; ++k) ok &= fabsf(Deg(EchoWrapPi(r[j][1].H[k] - r[0][1].H[k]))) < 10.0f && fabsf(Deg(EchoWrapPi(r[j][2].H[k] - r[0][2].H[k]))) < 10.0f;
        Check(ok, "anchor: the same path at 30 / 60 / 120 / 180 Hz (within one 30 Hz tick of its 270 deg/s turn, 10 degrees, at every sample)");
        for (j = 0, ok = 1; j < 4; ++j)
            ok &= fabsf(r[j][2].H[2]) < 1e-5f && fabsf(Deg(EchoWrapPi(r[j][2].H[7] - 1.5707964f))) < 2.5f && fabsf(Deg(EchoWrapPi(r[j][2].H[6] - 1.5707964f))) < 30.0f;
        sprintf_s(buf, sizeof buf, "anchor: standing, a held idle turn of 90 degrees: still for 1 s, then settles (%.1f deg off at 3.5 s, %.1f at 5 s)",
                  Deg(1.5707964f - r[0][2].H[6]), Deg(1.5707964f - r[0][2].H[7]));
        Check(ok, buf);
        for (j = 0, ok = 1; j < 4; ++j)
            for (k = 0; k < 8; ++k) ok &= fabsf(r[j][3].H[k]) < 1e-5f;
        Check(ok, "anchor: standing, a 30 degree idle turn: the places stay (no circling)");
    }
    // 14. v3.31 ECHO331: her busy moments (the decomp's player modes)
    {
        EchoBusyL b;
        memset(&b, 0, sizeof b);
        Check(EchoBusyStep(&b, 0, 0, 0, 0) == EBZ_NONE, "busy: nothing");
        Check(EchoBusyStep(&b, 0, 0, 1, 4) == EBZ_CANDLE && EchoBusyStep(&b, 0x6A, 0, 2, 4) == EBZ_CANDLE, "busy: a Holy Candle (P+0x218 set, item use 4)");
        Check(EchoBusyStep(&b, 0, 0, 1, 6) == EBZ_NONE, "busy: another item use (6): no");
        Check(EchoBusyStep(&b, 0x4F, 0x6D, 0, 0) == EBZ_PICKUP && EchoBusyStep(&b, 0x4E, 0x6D, 0, 0) == EBZ_PICKUP &&
                  EchoBusyStep(&b, 0x4E, 0x6E, 0, 0) == EBZ_PICKUP && EchoBusyStep(&b, 0, 0x6D, 0, 0) == EBZ_NONE,
              "busy: a floor pickup (mode 0x4F, clip 0x6D / 0x6E, then 0x4E) to its end");
        Check(EchoBusyStep(&b, 0x4E, 0x6D, 0, 0) == EBZ_NONE && EchoBusyStep(&b, 0x4E, 0x6D, 0, 0) == EBZ_NONE,
              "busy: Playable Eileen's possession fit (clip 109 with mode 0x4E directly): not a pickup");
        Check(EchoBusyStep(&b, 0x4F, 0x20, 0, 0) == EBZ_NONE, "busy: mode 0x4F with another clip: no");
    }
    // 15. v3.31 ECHO331: the replay of her last attack: repetitions, every window once per repetition, any rate
    {
        static const int kR[4] = { 30, 60, 120, 180 };
        const float len = 0.83f, win[5] = { 0.10f, 0.1111f, 0.30f, 0.82f, 0.83f };
        int j, ok = 1, rep;
        Check(Near(EchoRecPhase(len, 0.5f, &rep), 0.5f, 1e-6f) && rep == 0 && Near(EchoRecPhase(len, 1.0f, &rep), len, 1e-6f) && rep == 0 &&
                  Near(EchoRecPhase(len, 1.18f + 0.05f, &rep), 0.05f, 1e-4f) && rep == 1,
              "replay: in a repetition, its gap (the last pose held), the next one");
        for (j = 0; j < 4; ++j) {
            const float dt = 1.0f / (float)kR[j];
            int hits[6][5], k, w, last = 0;
            float t = 0.0f, local = 0.0f;
            memset(hits, 0, sizeof hits);
            for (k = 0; k < (int)(6.0f * (len + ECHO_REC_GAP_S) / dt) - 2; ++k) {  // as EchoBzTick
                const float prev = local;
                t += dt;
                local = EchoRecPhase(len, t, &rep);
                if (rep >= 6) break;
                if (rep != last) {
                    for (w = 0; w < 5; ++w) hits[last][w] += win[w] > prev && win[w] <= len + 1.0f;
                    last = rep;
                    for (w = 0; w < 5; ++w) hits[rep][w] += win[w] > -1.0f && win[w] <= local;
                } else {
                    for (w = 0; w < 5; ++w) hits[rep][w] += win[w] > prev && win[w] <= local;
                }
            }
            for (k = 0; k < 5; ++k)
                for (w = 0; w < 5; ++w) ok &= hits[k][w] == 1;
        }
        Check(ok, "replay: every recorded window struck exactly once per repetition at 30 / 60 / 120 / 180 Hz (5 repetitions)");
    }
    {   // v3.31 ECHO331: the cross-fade of two placed poses
        float a[16], b2[16], o[16], l0, l2;
        EchoIdent(a);
        EchoRotY(b2, 1.5707964f), b2[12] = 100.0f;
        EchoMatMix(o, a, b2, 0.5f);
        l0 = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]), l2 = sqrtf(o[8] * o[8] + o[9] * o[9] + o[10] * o[10]);
        Check(Near(l0, 1.0f, 1e-4f) && Near(l2, 1.0f, 1e-4f) && Near(atan2f(o[2], o[0]), 0.7853982f, 1e-3f) && Near(o[12], 50.0f, 1e-4f),
              "cross-fade: a 90 degree turn is blended (half way: 45 degrees, unit rows, the place half way)");
        EchoRotY(b2, 2.6f);
        EchoMatMix(o, a, b2, 0.4f);
        Check(!memcmp(o, a, sizeof a), "cross-fade: beyond 120 degrees the nearer pose");
    }
    {   // v3.31 ECHO-331-001: a hit window open for the whole recording (1.8 s) keeps every sample due at 90 a second,
        // stepped at 30 / 60 / 120 / 180 Hz, as EchoRecTick appends them
        static const int hz[4] = { 30, 60, 120, 180 };
        int q, ok = 1;
        for (q = 0; q < 4; ++q) {
            EchoRing r;
            float t;
            int due = 0, kept = 0;
            EchoRingInit(&r, ECHO_REC_HIT_N);
            for (t = 0.0f; t <= ECHO_REC_MAX_S + 1e-4f; t += 1.0f / (float)hz[q]) {
                if (EchoRingDue(&r, t, 1.0f / 90.0f)) {
                    ++due;
                    if (r.count < ECHO_REC_HIT_N) EchoRingPush(&r, t), ++kept;
                }
            }
            ok &= kept == due && r.count == due && (hz[q] < 60 || due > 64);  /* nothing dropped; above the old 64 cap at 60+ */
        }
        Check(ok, "a hit window open for the whole 1.8 s recording keeps every sample at 30 / 60 / 120 / 180 Hz (ECHO-331-001)");
    }
    // 16. v3.33 ECHO333: the user's "with weapons equipped 1 shadow stays transparent out of combat while the other two stay
    // solid": the strike rule, the calm ramp of 3 shadows (their own delays) and the body plan at every budget level
    {
        static const int kR[4] = { 30, 60, 120, 180 };
        static const float kDly[3] = { 0.15f, 0.40f, 0.60f };
        char buf[260];
        int j, lv, lk, i, ok = 1;
        Check(EchoWhyOf(1, 0, 2, 0, ETP_HOME, 0) == ESW_NONE && EchoWhyOf(1, 0, 0, 0, ETP_HOME, 0) == ESW_NONE &&
                  EchoWhyOf(1, 0, 3, 0, ETP_HOME, 0) == ESW_SWING && EchoWhyOf(1, 0, 0, 2, ETP_HOME, 0) == ESW_SWING &&
                  EchoWhyOf(0, 1, 0, 0, ETP_HOME, 0) == ESW_REPLAY && EchoWhyOf(0, 0, 3, 1, ETP_HOME, 0) == ESW_NONE &&
                  EchoWhyOf(1, 0, 0, 0, ETP_AWAY, 1) == ESW_AWAY && EchoWhyOf(1, 0, 0, 0, ETP_IN, 1) == ESW_TELEPORT &&
                  EchoWhyOf(1, 0, 0, 0, ETP_IN, 0) == ESW_NONE,
              "strike rule: her ready stance (mode 2, a weapon raised) and walking are no strike; a swing / window, a busy replay, a Cast "
              "stay / teleport to an enemy are (a teleport home is not)");
        for (j = 0; j < 4; ++j) {  // her script: weapon raised (mode 2) 0..1 s, a swing with a window 1.0..1.5 s, raised again to 6 s
            const float dt = 1.0f / (float)kR[j];
            EchoCalm c[3];
            float kAt[3] = { 0, 0, 0 }, kMid[3] = { 1, 1, 1 };
            int t;
            memset(c, 0, sizeof c);
            for (t = 0; t <= kR[j] * 6; ++t) {
                const float now = (float)t * dt;
                for (i = 0; i < 3; ++i) {
                    const float tr = now - kDly[i];  // its replayed sample of her
                    const int swing = tr > 1.0f && tr <= 1.5f, st = swing ? 3 : 2, n = swing && tr > 1.1f && tr < 1.3f;
                    EchoCalmStep(&c[i], EchoWhyOf(1, 0, st, n, ETP_HOME, 0) != ESW_NONE, dt);
                    if (fabsf(now - 1.95f) < 0.5f * dt) kMid[i] = c[i].k;
                }
                if (fabsf(now - 4.0f) < 0.5f * dt) for (i = 0; i < 3; ++i) kAt[i] = c[i].k;
            }
            ok &= kAt[0] == 1.0f && kAt[1] == 1.0f && kAt[2] == 1.0f && kMid[0] < 0.05f && kMid[1] < 0.05f && kMid[2] < 0.05f;
        }
        Check(ok, "calm: 3 shadows (delays 0.15 / 0.40 / 0.60 s), a weapon raised before and after one swing: all solid while they replay it, "
                  "all calm (k = 1) 2.5 s after it, at 30 / 60 / 120 / 180 Hz");
        {   // the 1-of-3 case: v3.32's rule at budget level 2 (the user's log: within seconds of most rooms with a weapon)
            EchoBodyPl p32[3], p33[3];
            for (i = 0; i < 3; ++i) {
                EchoBodyPlan332(&p32[i], 0, 2, i, EchoBodyOp(0, 1.0f), 1.0f, 2);
                EchoBodyPlan(&p33[i], 0, 2, i, EchoBodyOp(0, 1.0f), 1.0f, 2);
            }
            sprintf_s(buf, sizeof buf, "the 1-of-3 case reproduced: v3.32 at budget level 2, all calm: shadow 1 %.0f %%, shadows 2 / 3 %.0f / %.0f %% "
                      "(no prepass: her surfaces blend); v3.33: %.0f / %.0f / %.0f %%", 100.0f * p32[0].op, 100.0f * p32[1].op, 100.0f * p32[2].op,
                      100.0f * p33[0].op, 100.0f * p33[1].op, 100.0f * p33[2].op);
            Check(Near(p32[0].op, 0.45f, 1e-4f) && p32[1].op > 0.80f && p32[2].op > 0.80f && !p32[1].depth && p32[0].depth &&
                      Near(p33[0].op, 0.45f, 1e-4f) && Near(p33[1].op, 0.45f, 1e-4f) && Near(p33[2].op, 0.45f, 1e-4f) && p33[1].depth && p33[2].depth, buf);
        }
        for (lv = 0, ok = 1; lv <= 3; ++lv)  // every level, look and shadow: a calm body keeps its prepass; all three alike
            for (lk = 0; lk < 4; ++lk) {
                EchoBodyPl p[3];
                for (i = 0; i < 3; ++i) EchoBodyPlan(&p[i], lk, lv, i, EchoBodyOp(lk, 1.0f), 1.0f, 2);
                for (i = 0; i < 3; ++i) ok &= p[i].depth && !p[i].solid && Near(p[i].op, p[0].op, 1e-6f) && p[i].op <= 0.451f;
                ok &= p[1].noWep == (lv >= 1) && p[1].lite == (lv >= 2) && p[0].noWep == (lv >= 3);  // (the budget's other savings stay)
            }
        Check(ok, "body plan: calm, every budget level 0-3 and every look: each shadow keeps its depth prepass, all three at the same opacity "
                  "(<= 45 %); the budget still drops weapons (level 1 / 3) and takes the light look (level 2)");
        for (lk = 1, ok = 1; lk < 4; ++lk)  // a vision look: see-through at every calm value, striking too, every level
            for (lv = 0; lv <= 3; ++lv)
                for (i = 0; i < 11; ++i) {
                    const float k = (float)i / 10.0f, a = EchoBodyOp(lk, k);
                    EchoBodyPl p;
                    EchoBodyPlan(&p, lk, lv, 2, a, k, 2);
                    const float hiA = lk == 3 ? ECHO_SV_STRIKE : ECHO_VIS_STRIKE, loA = lk == 3 ? ECHO_SV_CALM : ECHO_VIS_CALM;  // v3.34 FX334
                    ok &= !p.solid && p.depth && a <= hiA + 1e-6f && a >= loA - 1e-6f && p.op <= hiA + 0.001f && p.op < 0.70f;
                }
        Check(ok, "vision looks (Thermal, night vision: striking 50 % .. calm 35 %; Spirit Vision v3.34: 62 .. 45 %): see-through at all "
                  "times (under 70 %), always with the prepass");
        {   // the normal look: striking = solid (one pass); a short fade (forming, k 0) keeps v3.32's rule (no prepass by default,
            // none on the 2nd / 3rd at level 2 with EchoDepth=1); EchoDepth=0: never a prepass (the log says so)
            EchoBodyPl a, b, c, d;
            EchoBodyPlan(&a, 0, 2, 1, EchoBodyOp(0, 0.0f), 0.0f, 2);
            EchoBodyPlan(&b, 0, 0, 1, 0.6f, 0.0f, 2);
            EchoBodyPlan(&c, 0, 2, 1, 0.6f, 0.0f, 1);
            EchoBodyPlan(&d, 0, 0, 0, EchoBodyOp(0, 1.0f), 1.0f, 0);
            Check(a.solid && a.op == 1.0f && !b.depth && !c.depth && !d.depth && d.op > 0.8f,
                  "body plan: striking is solid; a short fade keeps v3.32's rule; EchoDepth=0 never draws a prepass");
        }
    }
    printf("%s: %d checks, %d failed\n", g_fail ? "FAIL" : "PASS", g_n, g_fail);
    return g_fail ? 1 : 0;
}
