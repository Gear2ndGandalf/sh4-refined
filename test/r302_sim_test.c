// v3.50 R302: native test of src\r302_sim.h (test\build_r302_test.cmd): Room 302's look and walk for Henry and Eileen.
// The hand-over rule (Playable Eileen v1.24.4 without SH4PE_Room302Yield keeps everything; no PE = Refined; a PE that
// yields), the rows' values (index, snap, ini text, the first-start seed from PE's ini), the look step against a
// transcription of Playable Eileen v1.24.4's FpViewAngles / FpClampView / FpLookAfter (SH4EileenWeapons.cpp, read only),
// the stock path (all Game + Feel Off), the pitch never past a limit over many ticks, the walk factor, the export layout.
#include <stdio.h>
#include <stddef.h>
#include "../src/r302_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- a transcription of PE v1.24.4 (lines 19196-19269)
static float pe_lookLimit, pe_upLimit, pe_feel;
static struct { float yawRate, pitchRate; unsigned clamps, resets; } pe_ff;
static float pe_pitchNow;
static int PeViewAngles(const float *m, float *yaw, float *pitch)
{
    const float rl = sqrtf(m[0] * m[0] + m[2] * m[2]);
    float h;
    if (!_finite(rl) || rl < 0.5f)
        return 0;
    *yaw = atan2f(-m[2], m[0]);
    h = m[8] * sinf(*yaw) + m[10] * cosf(*yaw);
    *pitch = atan2f(-m[9], h);
    return _finite(*pitch) != 0;
}
static void PeClampView(float *m, float *pd)
{
    float yaw, p0, lo, hi, p1, d;
    int rolled;
    if (!PeViewAngles(m, &yaw, &p0) || !_finite(*pd))
        return;
    lo = pe_lookLimit < -1.5533f ? -1.5533f : pe_lookLimit;
    hi = pe_upLimit > 1.5533f ? 1.5533f : pe_upLimit;
    rolled = fabsf(m[1]) > 0.01f;
    if (p0 < lo - 0.004f || p0 > hi + 0.004f || rolled) {
        const float cx = m[1] * m[6] - m[2] * m[5], cy = m[2] * m[4] - m[0] * m[6], cz = m[0] * m[5] - m[1] * m[4];
        if (cx * m[8] + cy * m[9] + cz * m[10] > 0.0f) {
            const float pc = p0 < lo ? lo : (p0 > hi ? hi : p0);
            const float sy = sinf(yaw), cyw = cosf(yaw), sp = sinf(pc), cp = cosf(pc);
            m[0] = cyw; m[1] = 0.0f; m[2] = -sy;
            m[4] = sy * sp; m[5] = cp; m[6] = cyw * sp;
            m[8] = sy * cp; m[9] = -sp; m[10] = cyw * cp;
            pe_ff.resets++;
            p0 = pc;
        }
    }
    p1 = p0 + *pd;
    if (p1 < lo) p1 = lo;
    if (p1 > hi) p1 = hi;
    d = p1 - p0;
    if (fabsf(d - *pd) > 1e-5f)
        ++pe_ff.clamps;
    *pd = d;
    pe_ff.pitchRate = d;
    pe_pitchNow = p1;
}
static void PeLookAfter(float *m, float *pd, float *yd, float *by, float dt)  // g_active, g_fpOn, stage 1, body built
{
    if (!_finite(*pd) || !_finite(*yd))
        return;
    if (pe_feel > 0.0f) {
        const float k = 1.0f - expf(-dt / (0.04f * pe_feel));
        float y = pe_ff.yawRate + k * (*yd - pe_ff.yawRate), p = pe_ff.pitchRate + k * (*pd - pe_ff.pitchRate);
        if (*yd == 0.0f && fabsf(y) < 1e-5f) y = 0.0f;
        if (*pd == 0.0f && fabsf(p) < 1e-5f) p = 0.0f;
        *by += y - *yd;
        *yd = y;
        *pd = p;
    }
    pe_ff.yawRate = *yd;
    PeClampView(m, pd);
}

// ---------------------------------------------------------------- helpers
static unsigned g_rng = 12345u;
static float Rnd(float lo, float hi)
{
    g_rng = g_rng * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(g_rng >> 8) / 16777216.0f;
}
// A view from yaw, pitch (+ = up, any value) and roll; mirror = a left-handed set (never rebuilt)
static void View(float *m, float yaw, float pitch, float roll, int mirror)
{
    const float sy = sinf(yaw), cy = cosf(yaw), sp = sinf(pitch), cp = cosf(pitch), sr = sinf(roll), cr = cosf(roll);
    const float R[3] = { cy, 0.0f, -sy }, D[3] = { sy * sp, cp, cy * sp }, F[3] = { sy * cp, -sp, cy * cp };
    int i;
    memset(m, 0, 16 * sizeof(float));
    for (i = 0; i < 3; ++i) {
        m[i] = (cr * R[i] + sr * D[i]) * (mirror ? -1.0f : 1.0f);
        m[4 + i] = -sr * R[i] + cr * D[i];
        m[8 + i] = F[i];
    }
    m[12] = 100.0f, m[13] = -300.0f, m[14] = 50.0f, m[15] = 1.0f;
}
static int Same(float a, float b) { return a == b || (a != a && b != b); }

// ---------------------------------------------------------------- tests
static void TestOwner(void)
{
    static const struct {
        R3OwnIn in;
        unsigned bit;
        int own, anomaly;
        const char *what;
    } kCases[] = {
        // peMapped yieldExp decided mask lists sites avail atTick
        { { 0, 0, 0, 0, 0, R3S_STOCK, 1, 0 }, R302F_LOOK, R3_OWN_REFINED, 0, "no PE, stock sites: Refined's" },
        { { 0, 0, 0, 0, 0, R3S_OURS, 1, 1 }, R302F_LOOK, R3_OWN_REFINED, 0, "no PE, our hook already in: Refined's" },
        { { 0, 0, 0, 0, 0, R3S_OTHER, 1, 0 }, R302F_LOOK, R3_OWN_NONE, 1, "no PE, another mod's bytes: nobody + anomaly" },
        { { 0, 0, 0, 0, 0, R3S_STOCK, 0, 0 }, R302F_WALK, R3_OWN_NONE, 0, "no PE, Movement Speed's sites out: nobody" },
        { { 1, 0, 0, 0, 1, R3S_OTHER, 1, 0 }, R302F_LOOK, R3_OWN_PE, 0, "PE v1.24.4 (no yield export), its hooks in: PE's" },
        { { 1, 0, 0, 0, 1, R3S_OTHER, 1, 1 }, R302F_LOOK, R3_OWN_PE, 0, "PE v1.24.4 at the tick: still PE's" },
        { { 1, 0, 0, 0, 0, R3S_STOCK, 1, 0 }, R302F_LOOK, R3_OWN_PE, 0, "PE v1.24.4 whose look byte check failed: PE's (stays out)" },
        { { 1, 0, 0, 0, 0, R3S_OURS, 1, 0 }, R302F_WALK, R3_OWN_PE, 0, "PE v1.24.4, the walk: PE's (factor 1)" },
        { { 1, 1, 0, 0, 0, R3S_STOCK, 1, 0 }, R302F_LOOK, R3_OWN_WAIT, 0, "a yielding PE not yet decided at Init: wait" },
        { { 1, 1, 0, 0, 0, R3S_STOCK, 1, 1 }, R302F_LOOK, R3_OWN_PE, 1, "still undecided at the tick: PE's + anomaly" },
        { { 1, 1, 1, R302F_ALL, 0, R3S_STOCK, 1, 0 }, R302F_LOOK, R3_OWN_REFINED, 0, "PE yields look and walk: look Refined's" },
        { { 1, 1, 1, R302F_ALL, 0, R3S_OURS, 1, 0 }, R302F_WALK, R3_OWN_REFINED, 0, "PE yields look and walk: walk Refined's" },
        { { 1, 1, 1, R302F_WALK, 0, R3S_OTHER, 1, 0 }, R302F_LOOK, R3_OWN_PE, 0, "PE yields the walk only: the look stays PE's" },
        { { 1, 1, 1, R302F_WALK, 0, R3S_OURS, 1, 0 }, R302F_WALK, R3_OWN_REFINED, 0, "PE yields the walk only: walk Refined's" },
        { { 1, 1, 1, R302F_LOOK, 1, R3S_OTHER, 1, 0 }, R302F_LOOK, R3_OWN_NONE, 1, "PE yields the look but lists its site: nobody + anomaly" },
        { { 1, 1, 1, R302F_LOOK, 0, R3S_OTHER, 1, 0 }, R302F_LOOK, R3_OWN_NONE, 1, "PE yields, another mod's bytes: nobody + anomaly" },
        { { 1, 1, 1, R302F_ALL, 0, R3S_STOCK, 0, 0 }, R302F_WALK, R3_OWN_NONE, 0, "PE yields the walk, Movement Speed out: nobody" },
        { { 1, 1, 1, 0, 0, R3S_STOCK, 1, 1 }, R302F_LOOK, R3_OWN_PE, 0, "PE decided to keep both: PE's" },
    };
    int i, a;
    for (i = 0; i < (int)(sizeof kCases / sizeof kCases[0]); ++i) {
        const int o = R3DecideOwner(&kCases[i].in, kCases[i].bit, &a);
        CHECK(o == kCases[i].own && a == kCases[i].anomaly, "owner: %s: %s anomaly %d (want %s %d)", kCases[i].what, kR3OwnName[o], a,
              kR3OwnName[kCases[i].own], kCases[i].anomaly);
    }
    // the user's game today: every input with peMapped and no yield export answers PE, whatever else holds
    {
        int n = 0, bad = 0, s, l, av, t;
        for (s = 0; s < 3; ++s)
            for (l = 0; l < 2; ++l)
                for (av = 0; av < 2; ++av)
                    for (t = 0; t < 2; ++t) {
                        R3OwnIn in = { 1, 0, 0, 0, 0, 0, 0, 0 };
                        in.sites = s, in.peListsSite = l, in.avail = av, in.atTick = t;
                        bad += R3DecideOwner(&in, R302F_LOOK, &a) != R3_OWN_PE || a;
                        bad += R3DecideOwner(&in, R302F_WALK, &a) != R3_OWN_PE || a;
                        n += 2;
                    }
        CHECK(!bad, "owner: Playable Eileen without SH4PE_Room302Yield keeps both groups in all %d states (%d not)", n, bad);
    }
}

static void TestValues(void)
{
    int k, i, ex;
    char t[16];
    float v;
    for (k = 0; k < R3K_N; ++k)
        for (i = 0; i < kR3Count[k]; ++i) {
            const int j = R3IndexOf(k, R3Val(k, i), &ex);
            CHECK(j == i && ex, "values: %s row value %g -> index %d exact %d (want %d)", kR3Key[k], R3Val(k, i), j, ex, i);
            R3IniText(k, i, t, sizeof t);
            CHECK(R3ParseNum(t, &v) && R3IndexOf(k, v, &ex) == i && ex, "values: %s index %d writes \"%s\", read back the same", kR3Key[k], i, t);
        }
    CHECK(R3IndexOf(R3K_DOWN, 0.0f, &ex) == 0 && ex, "values: LookDownLimit 0 = the game's 50 (exact)");
    CHECK(R3IndexOf(R3K_UP, 0.0f, &ex) == 0 && ex, "values: LookUpLimit 0 = the game's 25 (exact)");
    CHECK(R3IndexOf(R3K_FEEL, 0.0f, &ex) == 0 && ex, "values: LookFeel 0 = Off (exact)");
    CHECK(R3IndexOf(R3K_WALK, 0.0f, &ex) == R3_WALK_GAME && !ex, "values: WalkSpeed 0 = the game's 1 (snapped)");
    CHECK(R3IndexOf(R3K_DOWN, 72.0f, &ex) == 3 && !ex, "values: LookDownLimit 72 -> 70 (snapped)");
    CHECK(R3IndexOf(R3K_DOWN, 30.0f, &ex) == 0 && !ex, "values: LookDownLimit 30 (PE allows 10..) -> the game's 50 (snapped)");
    CHECK(R3IndexOf(R3K_DOWN, 90.0f, &ex) == 6 && !ex, "values: LookDownLimit 90 -> 85");
    CHECK(R3IndexOf(R3K_UP, 50.0f, &ex) == 2 && !ex, "values: LookUpLimit 50 -> 45 (lower on a near tie: |45-50| < |60-50|)");
    CHECK(R3IndexOf(R3K_FEEL, 0.75f, &ex) == 1 && !ex, "values: LookFeel 0.75 -> 0.5 (a tie: the lower)");
    CHECK(R3IndexOf(R3K_FEEL, 1.25f, &ex) == 2 && !ex, "values: LookFeel 1.25 -> 1 (a tie: the lower)");
    CHECK(R3IndexOf(R3K_WALK, 0.65f, &ex) == 3 && ex, "values: WalkSpeed 0.65 (the user's) is a row value");
    CHECK(R3IndexOf(R3K_WALK, 1.5f, &ex) == 10 && !ex, "values: WalkSpeed 1.5 (PE's max) -> 1.2");
    CHECK(R3IndexOf(R3K_WALK, -1.0f, &ex) == R3_WALK_GAME && !ex, "values: WalkSpeed -1 -> the game's");
    R3IniText(R3K_DOWN, 0, t, sizeof t);
    CHECK(!strcmp(t, "0"), "values: Look Down Game is written 0 (PE's meaning), got %s", t);
    R3IniText(R3K_WALK, R3_WALK_GAME, t, sizeof t);
    CHECK(!strcmp(t, "1"), "values: Walk Speed Game is written 1, got %s", t);
    CHECK(R3ParseNum(" 85 ", &v) && v == 85.0f && R3ParseNum("0.65\r\n", &v) && fabsf(v - 0.65f) < 1e-6f, "values: numbers with spaces");
    CHECK(!R3ParseNum("", &v) && !R3ParseNum("abc", &v) && !R3ParseNum("85deg", &v) && !R3ParseNum(NULL, &v) && !R3ParseNum("nan", &v),
          "values: not numbers");
    // the seed
    CHECK(!R3Seed(R3K_DOWN, 0, 1, "70", t, sizeof t) && !strcmp(t, "85"), "seed: no PE ini: the default 85 (%s)", t);
    CHECK(R3Seed(R3K_DOWN, 1, 1, "85", t, sizeof t) && !strcmp(t, "85"), "seed: PE 85: 85");
    CHECK(R3Seed(R3K_UP, 1, 1, "70", t, sizeof t) && !strcmp(t, "70"), "seed: PE up 70: 70");
    CHECK(R3Seed(R3K_UP, 1, 1, "0", t, sizeof t) && !strcmp(t, "0"), "seed: PE up 0 (the game's): 0");
    CHECK(R3Seed(R3K_DOWN, 1, 1, "72", t, sizeof t) && !strcmp(t, "70"), "seed: PE 72: snapped 70 (%s)", t);
    CHECK(R3Seed(R3K_FEEL, 1, 1, "1", t, sizeof t) && !strcmp(t, "1"), "seed: PE Feel 1: LookFeel 1");
    CHECK(R3Seed(R3K_FEEL, 1, 1, "0", t, sizeof t) && !strcmp(t, "0"), "seed: PE Feel 0: LookFeel 0");
    CHECK(R3Seed(R3K_WALK, 1, 1, "0.65", t, sizeof t) && !strcmp(t, "0.65"), "seed: PE WalkSpeed 0.65: 0.65");
    CHECK(R3Seed(R3K_WALK, 1, 1, "1", t, sizeof t) && !strcmp(t, "1"), "seed: PE WalkSpeed 1: 1");
    CHECK(R3Seed(R3K_DOWN, 1, 0, "85", t, sizeof t) && !strcmp(t, "0"), "seed: PE FirstPersonBody 0: the game's (0), as PE acts");
    CHECK(R3Seed(R3K_FEEL, 1, 0, "1", t, sizeof t) && !strcmp(t, "0"), "seed: FirstPersonBody 0: LookFeel 0");
    CHECK(R3Seed(R3K_WALK, 1, 0, "0.65", t, sizeof t) && !strcmp(t, "1"), "seed: FirstPersonBody 0: WalkSpeed 1");
    CHECK(!R3Seed(R3K_WALK, 1, 1, "", t, sizeof t) && !strcmp(t, "0.65"), "seed: PE key missing: PE's own default 0.65");
    CHECK(!R3Seed(R3K_DOWN, 1, 1, "lots", t, sizeof t) && !strcmp(t, "85"), "seed: PE text not a number: the default");
    for (k = 0; k < R3K_N; ++k) {
        float d;
        CHECK(R3ParseNum(kR3Def[k], &d) && R3IndexOf(k, d, &ex) >= 0 && ex, "values: the default of %s (%s) is a row value", kR3Key[k], kR3Def[k]);
    }
}

static void TestStockPath(void)
{
    union { float f; unsigned u; } a, b;
    CHECK(!R3LookActive(0, 0, 0), "stock: all Game + Feel Off: the look hook does nothing");
    CHECK(R3LookActive(6, 0, 0) && R3LookActive(0, 6, 0) && R3LookActive(0, 0, 2), "stock: any other value acts");
    a.f = R3UpLimit(0), b.u = 0x3EDF66F3u;  // [0x5BD6D8] 0.43633232 (the immediate at 0x52DCE0 is the same bits)
    CHECK(a.u == b.u, "stock: Look Up Game = the stock 25 degrees bit for bit (%08X)", a.u);
    a.f = R3DownLimit(0), b.u = 0xBF5F66F3u;  // [0x5C5BC0] -0.87266463 (the immediate at 0x52DD2B)
    CHECK(a.u == b.u, "stock: Look Down Game = the stock -50 degrees bit for bit (%08X)", a.u);
    CHECK(fabsf(R3UpLimit(6) - 85.0f * R3_DEG) < 1e-6f && fabsf(R3DownLimit(6) + 85.0f * R3_DEG) < 1e-6f, "limits: 85 / 85");
    CHECK(fabsf(R3DownLimit(1) + 60.0f * R3_DEG) < 1e-6f && fabsf(R3UpLimit(1) - 35.0f * R3_DEG) < 1e-6f, "limits: 60 down, 35 up");
    CHECK(fabsf(R3Dt(60) - 1.0f / 60.0f) < 1e-7f && fabsf(R3Dt(0) - 1.0f / 30.0f) < 1e-7f && fabsf(R3Dt(500) - 1.0f / 30.0f) < 1e-7f,
          "dt: 60 fps, a bad rate reads 30");
}

static void TestVsPe(void)
{
    int run, bad = 0, n = 0, rebuilt = 0, clamped = 0, eased = 0, mism = 0;
    for (run = 0; run < 400; ++run) {
        float mA[16], mB[16];
        const int downIdx = (int)Rnd(0.0f, 6.999f), upIdx = (int)Rnd(0.0f, 6.999f), feelIdx = (int)Rnd(0.0f, 4.999f);
        const float feel = kR3Feel[feelIdx], dt = run & 1 ? 1.0f / 60.0f : 1.0f / 30.0f;
        const float lo = R3DownLimit(downIdx), hi = R3UpLimit(upIdx);
        float yaw = Rnd(-3.1f, 3.1f), pitch = Rnd(-1.7f, 1.7f), roll = run % 7 == 0 ? Rnd(-0.3f, 0.3f) : 0.0f;
        float byA = Rnd(-3.0f, 3.0f), byB = byA;
        R3Ease e = { 0.0f, 0.0f };
        int t;
        if (run % 11 == 0) pitch = Rnd(1.6f, 2.6f);  // upside down past the top
        View(mA, yaw, pitch, roll, run % 23 == 0);
        memcpy(mB, mA, sizeof mA);
        pe_lookLimit = lo, pe_upLimit = hi, pe_feel = feel;
        memset(&pe_ff, 0, sizeof pe_ff);
        for (t = 0; t < 40; ++t) {
            float pdA = Rnd(-0.12f, 0.12f), ydA = Rnd(-0.07f, 0.07f), pdB, ydB;
            R3StepOut o;
            if (t % 9 == 4) pdA = 0.0f, ydA = 0.0f;  // let go
            if (t > 30) pdA = run & 2 ? 0.1f : -0.1f;  // held at a limit
            pdB = pdA, ydB = ydA;
            PeLookAfter(mA, &pdA, &ydA, &byA, dt);
            R3LookStep(&e, mB, &pdB, &ydB, &byB, dt, feel, lo, hi, &o);
            ++n;
            rebuilt += o.rebuilt, clamped += o.clamped, eased += o.eased;
            if (!Same(pdA, pdB) || !Same(ydA, ydB) || !Same(byA, byB) || memcmp(mA, mB, sizeof mA) || !Same(pe_ff.yawRate, e.yawRate) ||
                !Same(pe_ff.pitchRate, e.pitchRate)) {
                if (mism++ < 5) printf("  run %d tick %d: PE pd %.9g yd %.9g by %.9g / R3 %.9g %.9g %.9g\n", run, t, pdA, ydA, byA, pdB, ydB, byB);
                ++bad;
            }
            if (o.clamped && !Same(o.pitch, pe_pitchNow)) ++bad;
        }
    }
    CHECK(!bad, "look vs Playable Eileen v1.24.4: %d ticks identical (deltas, body yaw, view, eased state); %d differ", n, bad);
    CHECK(rebuilt > 0 && clamped > 100 && eased > 100, "look: the cases cover rebuilds (%d), clamps (%d) and easing (%d)", rebuilt, clamped, eased);
}

static void TestLimitsHold(void)
{
    // the view's pitch after each tick = before + the delta R3LookStep leaves (the game turns the view by P+0x60)
    int k, t, bad = 0;
    for (k = 0; k < 4; ++k) {
        const float lo = R3DownLimit(k == 0 ? 6 : k), hi = R3UpLimit(k == 0 ? 6 : 6 - k);
        float m[16], by = 0.0f, pitch = 0.0f, yaw = 0.3f;
        R3Ease e = { 0.0f, 0.0f };
        for (t = 0; t < 600; ++t) {
            float pd = (t / 100) & 1 ? -0.15f : 0.15f, yd = 0.02f, p2, y2;
            R3StepOut o;
            View(m, yaw, pitch, 0.0f, 0);
            R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 60.0f, 1.0f, lo, hi, &o);
            pitch += pd;
            yaw += yd;
            View(m, yaw, pitch, 0.0f, 0);
            if (!R3ViewAngles(m, &y2, &p2) || p2 > hi + 1e-4f || p2 < lo - 1e-4f) ++bad;
        }
    }
    CHECK(!bad, "look: pushed up and down for 600 ticks, the pitch never passes a limit (%d past)", bad);
    {   // a rolled view comes back level at once; a mirrored one is left alone
        float m[16], pd = 0.0f, yd = 0.0f, by = 0.0f;
        R3Ease e = { 0.0f, 0.0f };
        R3StepOut o;
        View(m, 1.0f, 0.2f, 0.2f, 0);
        R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 60.0f, 0.0f, R3DownLimit(6), R3UpLimit(6), &o);
        CHECK(o.rebuilt && m[1] == 0.0f && fabsf(o.rollDeg) > 5.0f, "look: a rolled view is rebuilt without roll (%g deg)", o.rollDeg);
        View(m, 1.0f, 0.2f, 0.2f, 1);
        R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 60.0f, 0.0f, R3DownLimit(6), R3UpLimit(6), &o);
        CHECK(!o.rebuilt, "look: a left-handed view is never rebuilt");
    }
    {   // not finite: nothing changes
        float m[16], pd = (float)HUGE_VAL, yd = 0.01f, by = 1.0f;
        R3Ease e = { 0.0f, 0.0f };
        R3StepOut o;
        View(m, 0.0f, 0.0f, 0.0f, 0);
        R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 30.0f, 1.0f, -1.0f, 1.0f, &o);
        CHECK(yd == 0.01f && by == 1.0f && !o.eased && !o.clamped, "look: an infinite delta leaves everything as it is");
    }
    {   // Feel Off: the deltas pass untouched inside the limits
        float m[16], pd = 0.05f, yd = -0.03f, by = 0.5f;
        R3Ease e = { 0.0f, 0.0f };
        R3StepOut o;
        View(m, 0.0f, 0.0f, 0.0f, 0);
        R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 30.0f, 0.0f, R3DownLimit(6), R3UpLimit(6), &o);
        CHECK(pd == 0.05f && yd == -0.03f && by == 0.5f && !o.eased && !o.clamped && !o.rebuilt, "look: Feel Off inside the limits: untouched");
    }
    {   // Feel 1 at 60 fps: a held turn reaches 99 % of its speed within 0.2 s, a release stops within 0.25 s
        float m[16], by = 0.0f;
        R3Ease e = { 0.0f, 0.0f };
        int i, rise = -1, fall = -1;
        View(m, 0.0f, 0.0f, 0.0f, 0);
        for (i = 0; i < 60; ++i) {
            float pd = 0.0f, yd = i < 30 ? 0.05f : 0.0f;
            R3StepOut o;
            R3LookStep(&e, m, &pd, &yd, &by, 1.0f / 60.0f, 1.0f, R3DownLimit(6), R3UpLimit(6), &o);
            if (rise < 0 && yd > 0.0495f) rise = i;
            if (i >= 30 && fall < 0 && yd < 0.0025f) fall = i - 30;  // 5 per cent of the turn
        }
        CHECK(rise >= 0 && rise <= 12 && fall >= 0 && fall <= 8, "look: Feel 1 eases a turn in within %d ticks (99 %%) and out within %d (to 5 %%)", rise, fall);
    }
}

static void TestWalk(void)
{
    CHECK(R3WalkFactor(1, 1, 0.65f) == 0.65f, "walk: Refined's, Room 302: 0.65");
    CHECK(R3WalkFactor(1, 10, 0.65f) == 1.0f, "walk: stage 0x0A (the crawl): 1");
    CHECK(R3WalkFactor(1, 2, 0.65f) == 1.0f, "walk: a world: 1");
    CHECK(R3WalkFactor(0, 1, 0.65f) == 1.0f, "walk: Playable Eileen's (v1.24.4): 1 (no double scaling with her WalkSpeed)");
    CHECK(R3WalkFactor(1, 1, 0.0f) == 1.0f, "walk: a bad value: 1");
}

static void TestLayout(void)
{
    CHECK(offsetof(R3Export, magic) == 0 && offsetof(R3Export, offer) == 12 && offsetof(R3Export, decided) == 16 &&
          offsetof(R3Export, taken) == 20 && offsetof(R3Export, lookTicks) == 24 && offsetof(R3Export, yawRate) == 28 &&
          offsetof(R3Export, pitch) == 36 && offsetof(R3Export, walkSpeed) == 52 && sizeof(R3Export) == 56,
          "export: SH4R_Room302 layout as R302_CONTRACT.md");
    CHECK(offsetof(R3PeYield, decided) == 8 && offsetof(R3PeYield, yield) == 12 && offsetof(R3PeYield, peVersion) == 16 &&
          sizeof(R3PeYield) == 20, "export: SH4PE_Room302Yield layout as R302_CONTRACT.md");
    {
        const unsigned x = R3X_MAGIC, y = R3Y_MAGIC;
        CHECK(!memcmp("R3RF", &x, 4) && !memcmp("R302", &y, 4), "export: the magics read R3RF / R302 in memory");
    }
}

int main(void)
{
    TestOwner();
    TestValues();
    TestStockPath();
    TestVsPe();
    TestLimitsHold();
    TestWalk();
    TestLayout();
    printf("r302_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
