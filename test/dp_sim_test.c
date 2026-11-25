// v3.27.0 dp_sim.h native test (PLAN 10): the fog's damage over a scripted inside / outside sequence must be the same
// at 30 / 60 / 120 / 180 Hz and with a 30 Hz gate (the being stepped only on gate ticks with dt 1/30, as EntityGate);
// the ramp reaches the full rate after 10 s; decay, forget and pool reuse; the Eileen gate; the ghost hold.
// Build and run: test\build_dp_test.cmd (cl /O2 x86, as build.cmd).
#define _CRT_SECURE_NO_WARNINGS  // v3.41 DP341: sprintf without C4996 (the build stays warning-free)
#include <stdio.h>
#include <stdlib.h>
#include "../src/dp_sim.h"

static int g_fail;
static void Check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_fail;
}
// the scripted sequence: inside 0-14 s, outside 14-15.2 s, inside 15.2-20 s, outside 20-30 s
static int Inside(double t) { return (t < 14.0) || (t >= 15.2 && t < 20.0); }

// total fraction of max HP taken over 30 s at `hz` ticks a second; gate = step every (hz/30)th tick with dt 1/30
static double Run(int hz, int gate, int level, float *tinEnd)
{
    const DpLevel *L = &kDpLevel[level];
    float tin = 0.0f;
    double sum = 0.0;
    long k, n = 30L * hz, acc = 0;
    for (k = 0; k < n; ++k) {
        const double t = (double)k / hz;  // the tick's start time
        if (gate) {  // the 30 Hz gate: a step on the tick where the accumulator passes the rate (GateAdvance)
            acc += 30;
            if (acc < hz) continue;
            acc -= hz;
            sum += DpRampStep(&tin, Inside(t), 1.0f / 30.0f, L->lo, L->hi);
        } else {
            sum += DpRampStep(&tin, Inside(t), 1.0f / (float)hz, L->lo, L->hi);
        }
    }
    if (tinEnd) *tinEnd = tin;
    return sum;
}
// the closed form of the same sequence (seconds): ramp 0..10 inside, 10..14 at hi; out 1.2 s (tin 10 -> 7.6);
// 15.2..17.6 ramp 7.6 -> 10, 17.6..20 at hi
static double Expect(int level)
{
    const DpLevel *L = &kDpLevel[level];
    const double lo = L->lo, hi = L->hi, k = (hi - lo) / 10.0;
    double s = lo * 10.0 + k * 50.0 + hi * 4.0;
    s += lo * 2.4 + k * (100.0 - 7.6 * 7.6) / 2.0 + hi * 2.4;
    return s;
}

// ---- v3.41 DP341: the draw gate (DpGateStep)
static DpGateIn GIn(DpU32 room, int drawn, int peep, int warp, float x, float y, float z)
{
    DpGateIn in;
    in.room = room, in.roomDrawn = drawn, in.peep = peep, in.warp = warp;
    in.pos[0] = x, in.pos[1] = y, in.pos[2] = z;
    return in;
}
// steps a gate n frames of dt with the same input; returns the frames shown; *firstShow: the step index of the first
// shown frame (-1 none); *resets: resets seen
static int GRun(DpGate *g, const DpGateIn *in, float dt, int n, int *firstShow, int *resets)
{
    int k, shown = 0, r;
    if (firstShow) *firstShow = -1;
    for (k = 0; k < n; ++k) {
        if (DpGateStep(g, in, dt, &r)) {
            if (firstShow && *firstShow < 0) *firstShow = k;
            ++shown;
        }
        if (resets) *resets += r;
    }
    return shown;
}
static void TestGate(void)
{
    char b[240];
    static const int kHz[5] = { 30, 60, 120, 144, 180 };
    int h, first, resets, shown, r;
    DpGate g;
    DpGateIn in;
    // the start: a zeroed gate holds, then shows after 0.30 s (real time, any frame rate) and >= 4 frames, one reset
    for (h = 0; h < 5; ++h) {
        const float dt = 1.0f / (float)kHz[h];
        memset(&g, 0, sizeof g);
        in = GIn(0x00020001u, 1, 0, 0, 100.0f, 0.0f, -50.0f);
        resets = 0;
        shown = GRun(&g, &in, dt, kHz[h], &first, &resets);
        sprintf(b, "gate: %d fps: the start holds, shows after %.3f s (0.30 .. 0.30 + 1 frame), one reset, then stays (%d shown)",
                kHz[h], first * dt, shown);
        Check(first >= 0 && first * dt >= DP_GATE_SETTLE_S - dt - 1e-4f && first * dt <= DP_GATE_SETTLE_S + dt + 1e-4f &&
              first >= DP_GATE_SETTLE_F - 1 && resets == 1 && shown == kHz[h] - first && g.why == DPW_START, b);
    }
    memset(&g, 0, sizeof g);
    in = GIn(1, 1, 0, 0, 0.0f, 0.0f, 0.0f);
    GRun(&g, &in, 0.0f, 200, &first, NULL);
    Check(first < 0, "gate: frames with no time step (a gap) never end the settle");
    memset(&g, 0, sizeof g);
    in = GIn(1, 1, 0, 0, 0.0f, 0.0f, 0.0f);
    GRun(&g, &in, 0.1f, 3, &first, NULL);
    Check(first < 0, "gate: 0.3 s in 3 frames (10 fps) is not enough: at least 4 frames");
    GRun(&g, &in, 0.1f, 1, &first, NULL);
    Check(first == 0, "gate: ... the 4th frame shows");
    // the views: the room's geometry off (door scope, wall hole, window, Water Prison peep), the peeping flag
    {
        static const struct { int drawn, peep, why; const char *name; } kV[2] = {
            { 0, 0, DPW_ROOMOFF, "the room's geometry off (a view / a load)" }, { 1, 1, DPW_PEEP, "the Room 302 peeping flag" } };
        int v;
        for (v = 0; v < 2; ++v) {
            DpGateIn on = GIn(0x00010001u, kV[v].drawn, kV[v].peep, 0, -300.0f, 0.0f, 120.0f);
            memset(&g, 0, sizeof g);
            in = GIn(0x00010001u, 1, 0, 0, -300.0f, 0.0f, 120.0f);
            GRun(&g, &in, 1.0f / 60.0f, 60, NULL, NULL);
            r = DpGateStep(&g, &on, 1.0f / 60.0f, &resets);
            sprintf(b, "gate: %s hides the fog on its first frame (why %d)", kV[v].name, g.why);
            Check(!r && g.changed == 1 && g.why == kV[v].why && g.state == DPG_HOLD, b);
            resets = 0;
            shown = GRun(&g, &on, 1.0f / 60.0f, 600, NULL, &resets);  // 10 s of the view
            Check(shown == 0 && resets == 0 && !g.changed, "gate: ... nothing for the whole view (10 s), logged once");
            shown = GRun(&g, &in, 1.0f / 60.0f, 60, &first, &resets);
            sprintf(b, "gate: ... after the view: hidden 0.30 s more, then shown with a reset (first at frame %d)", first);
            Check(first >= 17 && first <= 19 && resets == 1 && shown == 60 - first, b);
        }
    }
    // a room change whose entry place reaches her position 3 frames late: nothing between the change and 0.30 s after
    // the jump, and the reset frame is at the new place
    for (h = 0; h < 5; ++h) {
        const float dt = 1.0f / (float)kHz[h];
        int k, bad = 0, resetAt = -1, jumpAt = 3;
        memset(&g, 0, sizeof g);
        in = GIn(0x00020004u, 1, 0, 0, 500.0f, 0.0f, 500.0f);
        GRun(&g, &in, dt, kHz[h], NULL, NULL);
        for (k = 0; k < 2 * kHz[h]; ++k) {
            const int late = k >= jumpAt;
            DpGateIn n = GIn(0x00020001u, 1, 0, 0, late ? -2000.0f : 500.0f, 0.0f, late ? 900.0f : 500.0f);
            const int s = DpGateStep(&g, &n, dt, &r);
            if (s && (!late || (k - jumpAt) * dt < DP_GATE_SETTLE_S - 1e-4f)) ++bad;
            if (r && resetAt < 0) resetAt = k;
        }
        sprintf(b, "gate: %d fps: a room change with a late entry place: nothing drawn before 0.30 s after the jump (%d bad), "
                   "reset at %.3f s after it", kHz[h], bad, (resetAt - jumpAt) * dt);
        Check(!bad && resetAt > jumpAt && (resetAt - jumpAt) * dt <= DP_GATE_SETTLE_S + dt + 1e-4f && g.why == DPW_JUMP, b);
    }
    {   // the room key alone (the 2:01 -> 2:00 -> 2:01 transit of a stage entry) holds
        memset(&g, 0, sizeof g);
        in = GIn(0x00020001u, 1, 0, 0, 0.0f, 0.0f, 0.0f);
        GRun(&g, &in, 1.0f / 60.0f, 60, NULL, NULL);
        in.room = 0x00020000u;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_ROOM && g.changed, "gate: a room key change hides (why: a room change)");
        in.room = 0x00020001u;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_ROOM && !g.changed, "gate: ... and the key back again restarts the settle");
    }
    {   // a jump in the same room hides; fast running (12 u a frame at 60 fps = 720 u/s) never does
        int k, hid = 0;
        memset(&g, 0, sizeof g);
        in = GIn(7, 1, 0, 0, 0.0f, 0.0f, 0.0f);
        GRun(&g, &in, 1.0f / 60.0f, 60, NULL, NULL);
        for (k = 0; k < 600; ++k) {
            in.pos[0] += 12.0f;
            in.pos[1] = 30.0f * sinf(0.1f * (float)k);  // stairs
            hid += !DpGateStep(&g, &in, 1.0f / 60.0f, &r);
        }
        Check(hid == 0, "gate: running (720 u/s, up and down stairs) never hides the fog");
        for (k = 0, hid = 0; k < 150; ++k) {
            in.pos[0] += 2652.0f / 15.0f;  // No Clip's Fly at its fastest, 15 fps
            hid += !DpGateStep(&g, &in, 1.0f / 15.0f, &r);
        }
        Check(hid == 0, "gate: No Clip's Fly at 2652 u/s (177 u a frame at 15 fps) never hides the fog");
        in.pos[2] += 151.0f;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_JUMP, "gate: a jump of 151 u in a frame hides (a teleport)");
        in.pos[2] += 149.0f;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_JUMP && g.t > 0.0f && g.frames == 1, "gate: ... a step of 149 u after it continues the settle");
    }
    {   // a pending move holds at most DP_GATE_WARP_MAX (an event may set one in a room and leave it), named once
        int k, capped = 0, firstShow = -1;
        memset(&g, 0, sizeof g);
        in = GIn(3, 1, 0, 1, 10.0f, 0.0f, 10.0f);
        for (k = 0; k < 300; ++k) {
            const int s = DpGateStep(&g, &in, 1.0f / 60.0f, &r);
            capped += g.warpCapped;
            if (s && firstShow < 0) firstShow = k;
        }
        sprintf(b, "gate: a pending move holds %.2f s at most (%.1f + the settle), named once (%d)", firstShow / 60.0f,
                DP_GATE_WARP_MAX, capped);
        Check(capped == 1 && firstShow >= 0 && firstShow / 60.0f >= DP_GATE_WARP_MAX + DP_GATE_SETTLE_S - 0.05f &&
              firstShow / 60.0f <= DP_GATE_WARP_MAX + DP_GATE_SETTLE_S + 0.05f, b);
        in.warp = 0;
        DpGateStep(&g, &in, 1.0f / 60.0f, &r);
        in.warp = 1;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_WARP && g.warpS < 0.1f, "gate: ... a new pending move after it cleared holds again");
    }
    {   // a position that is not finite, a NaN time step
        volatile float zero = 0.0f;
        const float nan = zero / zero;
        memset(&g, 0, sizeof g);
        in = GIn(3, 1, 0, 0, 0.0f, 0.0f, 0.0f);
        GRun(&g, &in, 1.0f / 60.0f, 60, NULL, NULL);
        in.pos[1] = nan;
        r = DpGateStep(&g, &in, 1.0f / 60.0f, &resets);
        Check(!r && g.why == DPW_BADPOS, "gate: a position that is not finite hides");
        in.pos[1] = 0.0f;
        r = DpGateStep(&g, &in, nan, &resets);
        Check(!r && g.t == 0.0f && g.prevOk, "gate: a NaN time step counts as 0 (and the last good position is kept)");
    }
    {
        int w, named = 1;
        for (w = 0; w < DPW_N; ++w) named &= DpGateWhy(w)[0] != '?';
        Check(named && DpGateWhy(DPW_N)[0] == '?' && DpGateWhy(-1)[0] == '?', "gate: every reason has a name");
    }
}

// v3.42 SM342: Death Presence off in Room 302 (stages 1 and 0x0A, the hidden key brings it back) and the view gate of Echo
// Doubles / After Shock (the fog's first three signals, the same settle; a zeroed gate shows; no other state)
static void TestR302View(void)
{
    char b[240];
    static const int kHz[5] = { 30, 60, 120, 144, 180 };
    DpU32 st;
    int h, k, in, enters, first, hidden, shown, leak;
    DpView g;
    for (st = 0; st <= 0x0Cu; ++st) {
        sprintf(b, "room 302: stage %X: off %d (key 0), off %d (key 1)", st, DpOffHere(st, 0), DpOffHere(st, 1));
        Check(DpOffHere(st, 0) == (st == 1u || st == 0x0Au) && !DpOffHere(st, 1) && DpIsRoom302(st) == (st == 1u || st == 0x0Au), b);
    }
    {   // one line per entry: 2 -> 1 -> 1 -> 0x0A -> 1 -> 3 -> 1 enters twice; the first stage seen counts as an entry
        static const DpU32 kSeq[] = { 2, 1, 1, 0x0A, 1, 3, 3, 1, 1 };
        in = -1, enters = 0;
        for (k = 0; k < (int)(sizeof kSeq / sizeof kSeq[0]); ++k) enters += DpR302Enter(&in, kSeq[k]);
        Check(enters == 2 && in == 1, "room 302: one line per entry (2 entries; 1 <-> 0x0A is the same Room 302)");
        in = -1;
        Check(DpR302Enter(&in, 1) == 1 && DpR302Enter(&in, 1) == 0, "room 302: a start inside it is an entry");
    }
    // the view gate: a zeroed gate shows at once; a view hides on its first frame; back 0.30 s and >= 4 frames after it
    for (h = 0; h < 5; ++h) {
        const float dt = 1.0f / (float)kHz[h];
        int sig;
        for (sig = 0; sig < 3; ++sig) {
            memset(&g, 0, sizeof g);
            Check(DpViewStep(&g, 1, 0, 0, dt) == 1 && !g.changed && !g.resumed, "view: a zeroed gate shows");
            first = -1, hidden = 0, shown = 0, leak = 0;
            for (k = 0; k < 3 * kHz[h]; ++k) {  // 1 s of the view, then 2 s of play
                const int view = k < kHz[h];
                const int r = DpViewStep(&g, !(view && sig == 0), view && sig == 1, view && sig == 2, dt);
                if (k == 0) {
                    sprintf(b, "view: %d fps, signal %d: hidden on its first frame, logged once (%s)", kHz[h], sig, DpGateWhy(g.why));
                    Check(r == 0 && g.changed && g.why == (sig == 0 ? DPW_ROOMOFF : sig == 1 ? DPW_PEEP : DPW_WARP), b);
                }
                hidden += g.changed, shown += g.resumed;
                if (!view && r && first < 0) first = k - kHz[h];
                if (view && r) leak = 1;
            }
            sprintf(b, "view: %d fps, signal %d: shown again %.3f s after the view (0.30 .. +1 frame), once; stays", kHz[h], sig,
                    first * dt);
            Check(!leak && first >= 0 && first * dt >= DP_GATE_SETTLE_S - dt - 1e-4f && first * dt <= DP_GATE_SETTLE_S + dt + 1e-4f &&
                  first + 1 >= DP_GATE_SETTLE_F && hidden == 1 && shown == 1 && g.state == DPV_SHOW, b);
        }
    }
    memset(&g, 0, sizeof g);  // a pending move that stays set: hidden at most 2 s, then shown (after the settle)
    first = -1, shown = 0;
    for (k = 0; k < 4 * 60; ++k) {
        const int r = DpViewStep(&g, 1, 0, 1, 1.0f / 60.0f);
        shown += g.warpCapped;
        if (r && first < 0) first = k;
    }
    sprintf(b, "view: a pending move held at most 2 s (shown at %.2f s), its cap said once", first / 60.0f);
    Check(first >= 0 && first / 60.0f >= DP_GATE_WARP_MAX + DP_GATE_SETTLE_S - 0.05f && first / 60.0f <= DP_GATE_WARP_MAX + DP_GATE_SETTLE_S + 0.05f &&
          shown == 1, b);
    memset(&g, 0, sizeof g);
    DpViewStep(&g, 0, 0, 0, 1.0f / 60.0f);
    for (k = 0, shown = 0; k < 200; ++k) shown += DpViewStep(&g, 1, 0, 0, 0.0f);
    Check(shown == 0, "view: frames with no time step (a gap) never end the settle");
    memset(&g, 0, sizeof g);
    DpViewStep(&g, 0, 0, 0, 1.0f / 60.0f);
    for (k = 0, shown = 0; k < 3; ++k) shown += DpViewStep(&g, 1, 0, 0, 0.1f);
    Check(shown == 0, "view: 0.3 s in 3 frames (10 fps) is not enough: at least 4 frames");
    Check(DpViewStep(&g, 1, 0, 0, 0.1f) == 1 && g.resumed, "view: the 4th frame shows");
    memset(&g, 0, sizeof g);
    DpViewStep(&g, 0, 0, 0, 1.0f / 60.0f);
    for (k = 0; k < 10; ++k) DpViewStep(&g, 1, 0, 0, 1.0f / 60.0f);  // a view that comes back during the settle: starts again
    Check(DpViewStep(&g, 1, 1, 0, 1.0f / 60.0f) == 0 && !g.changed && g.t == 0.0f && g.why == DPW_PEEP,
          "view: a signal during the settle holds again (no second hide line)");
    {
        volatile float z = 0.0f;
        const float nanv = z / z;
        memset(&g, 0, sizeof g);
        DpViewStep(&g, 0, 0, 0, nanv);
        Check(g.heldS == 0.0f && g.state == DPV_HIDE, "view: a NaN dt is a gap (no time)");
        DpViewStep(&g, 1, 0, 0, 5.0f);
        Check(g.t == 0.25f, "view: a long dt counts at most 0.25 s");
    }
}

int main(void)
{
    int level, i;
    char b[200];
    TestGate();  // v3.41 DP341
    TestR302View();  // v3.42 SM342
    for (level = 1; level < DP_LEVELS; ++level) {
        const double ex = Expect(level);
        double r[5];
        static const int kHz[4] = { 30, 60, 120, 180 };
        for (i = 0; i < 4; ++i) r[i] = Run(kHz[i], 0, level, NULL);
        r[4] = Run(144, 1, level, NULL);
        for (i = 0; i < 5; ++i) {
            const double rel = fabs(r[i] - ex) / ex;
            sprintf(b, "level %d %s: %.6f of max HP over 30 s (closed form %.6f, rel %.2e)", level,
                    i < 4 ? (i == 0 ? "30 Hz" : i == 1 ? "60 Hz" : i == 2 ? "120 Hz" : "180 Hz") : "144 Hz gated 30", r[i], ex, rel);
            Check(rel < 2e-4, b);  // float sums of up to 5400 steps
        }
    }
    {   // the ramp: 10 s inside -> the full rate; 1 s outside -> 8 s
        float tin = 0.0f;
        int k;
        for (k = 0; k < 600; ++k) DpRampStep(&tin, 1, 1.0f / 60.0f, 0.02f, 0.08f);
        Check(fabsf(tin - 10.0f) < 1e-4f && DpRampFrac(tin) == 1.0f, "ramp: full after 10 s inside");
        for (k = 0; k < 60; ++k) DpRampStep(&tin, 0, 1.0f / 60.0f, 0.02f, 0.08f);
        Check(fabsf(tin - 8.0f) < 1e-3f, "ramp: 2 s back after 1 s outside");
        Check(DpRampStep(&tin, 1, 0.0f, 0.02f, 0.08f) == 0.0f && DpRampStep(&tin, 1, -1.0f, 0.02f, 0.08f) == 0.0f,
              "ramp: no damage for dt <= 0");
    }
    {   // the table: reuse, forget, full table
        static DpTable t;
        int a = 1, w1 = 2, w2 = 3, k;
        DpVictim *v;
        DpTableReset(&t, 7);
        v = DpFind(&t, &a, &w1);
        v->tin = 5.0f;
        Check(DpFind(&t, &a, &w1) == v && v->tin == 5.0f, "table: the same being keeps its state");
        v = DpFind(&t, &a, &w2);
        Check(v && v->tin == 0.0f && v->work == &w2, "table: a reused task (new work) starts clean");
        t.clock += DP_FORGET_S + 0.1f;
        {
            int other;
            DpFind(&t, &other, &w1);
        }
        for (k = 0; k < DP_VICTIMS; ++k)
            if (t.v[k].task == &a) break;
        Check(k == DP_VICTIMS, "table: a being unseen for 3 s is forgotten");
        {
            static int many[DP_VICTIMS + 5];
            DpTableReset(&t, 8);
            for (k = 0; k < DP_VICTIMS + 5; ++k) t.clock += 0.01f, DpFind(&t, &many[k], &w1);
            Check(t.drops == 5, "table: over 64 beings the stalest slot is taken (5 drops)");
        }
        Check(DpFind(&t, NULL, &w1) == NULL, "table: no task, no slot");
        {   // review DP-R401: both addresses reused after the being was gone from the list for a while
            int ta = 7, wa = 8;
            DpTableReset(&t, 9);
            v = DpFind(&t, &ta, &wa);
            v->tin = 6.0f, v->blowSent = 1;
            t.clock += 0.5f;  // not visited for 0.5 s (removed), then the same task and work addresses again
            v = DpFind(&t, &ta, &wa);
            Check(v && v->tin == 0.0f && !v->blowSent, "table: the same task + work after a 0.5 s gap is a new being");
        }
    }
    {   // review DP-I002: the caller pattern: visited every tick, 10 s inside, then outside: forgotten after 3 s outside
        static DpTable t;
        int task = 1, work = 2, k;
        DpVictim *v;
        float tinAt4 = -1.0f;
        DpTableReset(&t, 1);
        for (k = 0; k < 60 * 15; ++k) {
            const int in = k < 600;
            t.clock += 1.0f / 60.0f;
            v = DpFind(&t, &task, &work);
            DpVictimSeen(v, in, t.clock);
            DpRampStep(&v->tin, in, 1.0f / 60.0f, 0.02f, 0.08f);
            if (k == 600 + 4 * 60) tinAt4 = v->tin;
        }
        Check(tinAt4 == 0.0f, "victim: 4 s outside (visited every tick) = exposure forgotten (ramp restarts at 0)");
    }
    {   // review DP-I003: pending damage accrued inside survives a step out between update ticks (gated pattern)
        DpVictim v;
        float applied = 0.0f;
        int k;
        memset(&v, 0, sizeof v);
        for (k = 0; k < 8; ++k) {  // 120 Hz ticks, updates every 4th; inside for ticks 0-5, out from 6
            const int in = k < 6, upd = (k % 4) == 3;
            v.pend += DpRampStep(&v.tin, in, 1.0f / 120.0f, 0.02f, 0.08f);
            if (upd && v.pend > 0.0f) applied += v.pend, v.pend = 0.0f;
        }
        Check(fabsf(applied - 6.0f / 120.0f * 0.02f - 0.06f / 10.0f * (6.0f / 120.0f) * (6.0f / 120.0f) / 2.0f) < 1e-7f,
              "pend: damage accrued inside is applied at the next update tick after stepping out");
    }
    {   // Eileen: no damage while down, 5 s grace after
        DpEileen e;
        int k, n = 0;
        memset(&e, 0, sizeof e);
        e.tin = 6.0f;
        Check(!DpEileenGate(&e, 1, 1.0f / 60.0f) && e.tin == 0.0f, "eileen: down = no damage, her ramp restarts");
        for (k = 0; k < 600; ++k) n += DpEileenGate(&e, 0, 1.0f / 60.0f);
        Check(n >= 299 && n <= 301, "eileen: 5 s grace after she is up (then damage again)");
    }
    {   // the ghost hold
        DpVictim v;
        memset(&v, 0, sizeof v);
        Check(DpHoldStep(&v, 1, 30.0f, 9) == 30.0f && v.hold && v.held == 30.0f, "hold: the first tick takes the countdown and writes it");
        Check(DpHoldStep(&v, 1, 29.9f, 9) == 30.0f, "hold: the decrement is undone");
        Check(DpHoldStep(&v, 1, 29.9f, 10) == 29.9f && v.held == 29.9f, "hold: a new hit serial takes the new value");
        Check(DpHoldStep(&v, 1, 60.0f, 10) == 60.0f && v.held == 60.0f, "hold: a raised countdown is taken");
        Check(DpHoldStep(&v, 0, 59.9f, 10) < 0.0f && !v.hold, "hold: outside the fog it counts again");
        Check(DpHoldStep(&v, 1, 0.01f, 11) == DP_HOLD_MIN, "hold: a countdown caught at 0.01 s is held at 0.1 s (review DP-I201)");
    }
    {   // avoidance: a hit or an attack commits; the cycle alternates; the cut grows with depth
        DpAvoid a;
        DpU32 rng = 1;
        const DpAvoidK *k = &kDpAvoid[2];
        int s, changes = 0, last;
        memset(&a, 0, sizeof a);
        DpAvoidStep(&a, k, 1, 0, 0, 1.0f / 60.0f, &rng);
        Check(a.armed && a.phase == AV_HESITATE, "avoid: the first entry hesitates");
        Check(DpAvoidCut(&a, k, 400.0f, 399.0f) < 0.01f && fabsf(DpAvoidCut(&a, k, 400.0f, 200.0f) - k->hMax) < 1e-6f,
              "avoid: no cut at the rim, the full cut at 0.3 R inside");
        DpAvoidStep(&a, k, 1, 0, 1, 1.0f / 60.0f, &rng);
        Check(a.phase == AV_COMMIT && a.t >= 1.0f, "avoid: hit by her = commit");
        last = a.phase;
        for (s = 0; s < 60 * 60; ++s) {
            DpAvoidStep(&a, k, 1, 0, 0, 1.0f / 60.0f, &rng);
            if (a.phase != last) ++changes, last = a.phase;
        }
        Check(changes >= 10 && changes <= 40, "avoid: hesitate / commit alternate (10-40 changes a minute)");
        for (s = 0; s < 120; ++s) DpAvoidStep(&a, k, 0, 0, 0, 1.0f / 60.0f, &rng);
        Check(!a.armed, "avoid: re-armed after 1.5 s outside");
    }
    {   // v3.27.1 B4: the stain: inside 0-12 s (the ramp), outside after; equal at every rate; the half-life; the floor
        static const int kHz[4] = { 30, 60, 120, 180 };
        static const double kAt[5] = { 6.0, 13.0, 15.0, 20.0, 29.0 };
        double v[4][5];
        int h, j;
        float sv = 0.0f;
        for (h = 0; h < 4; ++h) {
            float tin = 0.0f, s = 0.0f;
            long k, n = 30L * kHz[h];
            const float dt = 1.0f / (float)kHz[h];
            j = 0;
            for (k = 0; k < n; ++k) {
                const double t = (double)k / kHz[h];
                const int in = t < 12.0;
                while (j < 5 && t >= kAt[j] - 1e-9) v[h][j++] = s;
                DpRampStep(&tin, in, dt, 0.05f, 0.20f);
                DpStainStep(&s, in, DpRampFrac(tin), dt);
            }
        }
        for (j = 0; j < 5; ++j) {
            double mx = 0.0;
            for (h = 1; h < 4; ++h) mx = fabs(v[h][j] - v[0][j]) > mx ? fabs(v[h][j] - v[0][j]) : mx;
            sprintf(b, "stain: equal at 30 / 60 / 120 / 180 Hz at %.0f s (s %.4f, max diff %.1e; 1 tick of ramp allowed)", kAt[j],
                    v[0][j], mx);
            Check(mx < (j == 0 ? 2e-3 : 1e-4), b);
        }
        Check(fabs(v[3][0] - 0.6) < 0.01, "stain: follows the ramp inside (0.6 at 6 s)");
        Check(fabs(v[3][1] - exp2(-1.0 / 3.0)) < 2e-3, "stain: 1 s outside after the full ramp = 2^(-1/3)");
        Check(fabs(v[3][2] - 0.5) < 2e-3, "stain: half after 3 s outside");
        Check(v[3][4] < DP_STAIN_MIN && v[3][3] > DP_STAIN_MIN, "stain: still shown 8 s out, gone (< 0.02) 17 s out");
        sv = 0.7f;
        DpStainStep(&sv, 1, 0.2f, 1.0f / 60.0f);
        Check(sv > 0.69f, "stain: a re-entry with a low ramp keeps the higher stain");
        sv = -1.0f;
        DpStainStep(&sv, 0, 0.0f, 0.1f);
        Check(sv == 0.0f, "stain: a bad value resets to 0");
    }
    printf(g_fail ? "%d FAILED\n" : "all passed\n", g_fail);
    return g_fail ? 1 : 0;
}
