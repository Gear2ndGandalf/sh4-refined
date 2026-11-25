// v3.43 DF343: native test of src\dof_sim.h (test\build_dof_test.cmd): the ini values, the slice plans for every focus
// and strength against an emulation of the stencil marking (every view depth gets the level its distance from the focus
// asks for; the blur never falls as the distance grows), the projection guard, the focus filter, the gate, the focus
// pick (cut-scene focus, player, first person), the depth probe's verdicts, the preview's mode, the card's sample depth.
// v3.47 DF347: the ramps (monotone in depth, no step over a quarter level, the first blur a quarter, each ramp centred on
// v3.46's step), the composite's draws (every code a blend of two levels), the pause's depth map (any strength's codes
// from it as from the depth), the card, the draws a frame.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/dof_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// the game's camera (0x41CB60 with n 50, f 40000)
static const float kN = 50.0f, kF = 40000.0f;
static float P22(void) { return -kN / (kF - kN); }
static float P32(void) { return kF * kN / (kF - kN); }

static void TestParse(void)
{
    CHECK(DofParseMode("Off") == DOF_OFF && DofParseMode("0") == DOF_OFF && DofParseMode("none") == DOF_OFF, "Off");
    CHECK(DofParseMode("Cut-scenes") == DOF_CUT && DofParseMode("cutscenes") == DOF_CUT && DofParseMode("Cut scenes") == DOF_CUT &&
          DofParseMode("1") == DOF_CUT, "Cut-scenes");
    CHECK(DofParseMode("Always") == DOF_ALWAYS && DofParseMode("ON") == DOF_ALWAYS && DofParseMode("2") == DOF_ALWAYS, "Always");
    CHECK(DofParseMode("") == -1 && DofParseMode(NULL) == -1 && DofParseMode("3") == -1 && DofParseMode("Alway") == -1 &&
          DofParseMode("Alwayss") == -1, "not values");
    // v3.45 QS345: Low / Medium / High (0 / 1 / 2); legacy (no marker): v3.43 / v3.44's Low / 0 = Medium, High / 1 = High
    CHECK(DofParseStrength("Low", 0) == DOFS_LOW && DofParseStrength("medium", 0) == DOFS_MEDIUM && DofParseStrength("high", 0) == DOFS_HIGH &&
          DofParseStrength("0", 0) == DOFS_LOW && DofParseStrength("1", 0) == DOFS_MEDIUM && DofParseStrength("2", 0) == DOFS_HIGH, "strength");
    CHECK(DofParseStrength("Low", 1) == DOFS_MEDIUM && DofParseStrength("0", 1) == DOFS_MEDIUM && DofParseStrength("High", 1) == DOFS_HIGH &&
          DofParseStrength("1", 1) == DOFS_HIGH && DofParseStrength("Medium", 1) == DOFS_MEDIUM, "strength, legacy");
    CHECK(DofParseStrength("Strong", 0) == -1 && DofParseStrength("", 0) == -1 && DofParseStrength(NULL, 0) == -1 && DofParseStrength("3", 1) == -1,
          "strength not values");
    CHECK(DOFS_N == 3 && !strcmp(kDofStrName[DOFS_LOW], "Low") && !strcmp(kDofStrName[DOFS_MEDIUM], "Medium") && !strcmp(kDofStrName[DOFS_HIGH], "High"),
          "strength names");
    {   // the one-time migration
        int rw, mk, v;
        v = DofStrMigrate("Low", 0, &rw, &mk);  // the user's v3.44 ini
        CHECK(v == DOFS_MEDIUM && rw && mk, "v3.44 Low -> Medium, rewritten, marked");
        v = DofStrMigrate("0", 0, &rw, &mk);
        CHECK(v == DOFS_MEDIUM && rw && mk, "v3.44 0 -> Medium");
        v = DofStrMigrate("High", 0, &rw, &mk);
        CHECK(v == DOFS_HIGH && !rw && mk, "v3.44 High stays High (no rewrite), marked");
        v = DofStrMigrate("1", 0, &rw, &mk);
        CHECK(v == DOFS_HIGH && rw && mk, "v3.44 1 -> written as High (1 would read Medium after the marker)");
        v = DofStrMigrate("Medium", 0, &rw, &mk);
        CHECK(v == DOFS_MEDIUM && !rw && mk, "no marker, already Medium (a fresh v3.45 ini): only marked");
        v = DofStrMigrate("bogus", 0, &rw, &mk);
        CHECK(v == -1 && !rw && mk, "not a value: not rewritten, marked");
        v = DofStrMigrate("Low", DOF_STR_VER, &rw, &mk);
        CHECK(v == DOFS_LOW && !rw && !mk, "after the marker a Low stays Low");
        v = DofStrMigrate("1", DOF_STR_VER, &rw, &mk);
        CHECK(v == DOFS_MEDIUM && !rw && !mk, "after the marker 1 = Medium");
        v = DofStrMigrate("High", 400, &rw, &mk);
        CHECK(v == DOFS_HIGH && !rw && !mk, "a later marker");
        v = DofStrMigrate(kDofStrName[DofStrMigrate("Low", 0, &rw, &mk)], DOF_STR_VER, &rw, &mk);
        CHECK(v == DOFS_MEDIUM && !rw && !mk, "the rewritten value read at the next start: Medium, nothing to do");
    }
    CHECK(!strcmp(kDofModeName[DofParseMode("cutscenes")], "Cut-scenes"), "the name written back");
}

// ---------------------------------------------------------------- v3.47 DF347: the ramps
// v3.46's tables (the steps the ramps are centred on): far ratios, near ratios, the whole level of each code
static const float kV346FarR[5] = { 1.25f, 1.6f, 2.2f, 3.2f, 5.0f };
static const float kV346NearR[2] = { 0.7f, 0.5f };
static const unsigned char kV346Lv[DOFS_N][11] = {
    { 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1 }, { 0, 0, 1, 1, 2, 2, 0, 0, 0, 0, 1 }, { 0, 1, 2, 3, 3, 3, 0, 0, 0, 1, 2 } };
static int V346Level(int s, float r)  // v3.46's whole level at w / F = r
{
    int k, code = 0;
    for (k = 1; k <= 5; ++k)
        if (r > kV346FarR[k - 1]) code = k;
    for (k = 1; k <= 2; ++k)
        if (r < kV346NearR[k - 1]) code = 8 + k;
    return kV346Lv[s][code];
}
static float V346Step(int s, int side, int L)  // the ratio where v3.46 reached level L on a side (0: never)
{
    int k;
    if (side == DOF_FAR) {
        for (k = 1; k <= 5; ++k)
            if (kV346Lv[s][k] >= L) return kV346FarR[k - 1];
    } else {
        for (k = 1; k <= 2; ++k)
            if (kV346Lv[s][8 + k] >= L) return kV346NearR[k - 1];
    }
    return 0.0f;
}
// the code a view depth w should get on a side (the count of the side's slices it lies beyond)
static int WantCode(int s, int side, float F, float w)
{
    int c, code = 0;
    for (c = 1; c <= DofRampN(s, side); ++c) {
        const float t = DofSliceW(F, s, side, c);
        if (side == DOF_FAR ? w > t : w < t) code = c;
    }
    return code;
}

static void TestTables(void)
{
    int s, sd, c;
    for (s = 0; s < DOFS_N; ++s)
        for (sd = 0; sd < DOF_SIDES; ++sd) {
            const DofRampT *r = &kDofRamp[s][sd];
            CHECK(r->n >= DOF_Q && r->n <= DOF_SIDE_N && r->n % DOF_Q == 0, "%s %s: whole levels of quarter steps (%d)", kDofStrName[s],
                  kDofSideName[sd], r->n);
            CHECK(r->n / DOF_Q <= DOF_LEVELS, "%s %s: at most the eighth level", kDofStrName[s], kDofSideName[sd]);
            for (c = 0; c < r->n; ++c) {
                CHECK(sd == DOF_FAR ? r->g[c] > 0 : r->g[c] < 0, "%s %s slice %d on its side of the focus", kDofStrName[s], kDofSideName[sd], c + 1);
                if (c) CHECK(sd == DOF_FAR ? r->g[c] > r->g[c - 1] : r->g[c] < r->g[c - 1], "%s %s: the slices go outwards", kDofStrName[s],
                             kDofSideName[sd]);
                CHECK(DofCodeQ(s, sd, c + 1) == c + 1, "code c = c quarters");
            }
            // the same deepest level as v3.46 on both sides
            {
                int Lmax = 0, L;
                for (L = 1; L <= DOF_LEVELS; ++L)
                    if (V346Step(s, sd, L) > 0.0f) Lmax = L;
                CHECK(r->n / DOF_Q == Lmax, "%s %s: reaches v3.46's deepest level %d (%d)", kDofStrName[s], kDofSideName[sd], Lmax, r->n / DOF_Q);
            }
        }
    CHECK(DofCodeQ(DOFS_LOW, DOF_FAR, 0) == 0 && DofCodeQ(DOFS_LOW, DOF_FAR, 5) == 0 && DofCodeQ(-1, 0, 1) == 0 && DofCodeQ(DOFS_N, 0, 1) == 0 &&
          DofCodeQ(0, 2, 1) == 0, "not codes");
    CHECK(DofLevelLo(0, 1) == 0 && DofLevelLo(3, 1) == 0 && DofLevelLo(4, 1) == 1 && DofLevelLo(7, 1) == 1 && DofLevelLo(12, 1) == 3 &&
          DofLevelLo(15, 1) == 3, "blend: the level below");
    CHECK(DofLevelLo(1, 0) == 0 && DofLevelLo(2, 0) == 1 && DofLevelLo(5, 0) == 1 && DofLevelLo(6, 0) == 2 && DofLevelLo(12, 0) == 3 &&
          DofLevelLo(15, 0) == 3, "no blend: the nearest whole level");
    CHECK(fabsf(DofGridR(24) - 2.0f) < 1e-5f && fabsf(DofGridR(-24) - 0.5f) < 1e-6f && DofGridR(0) == 1.0f, "the grid: 24 steps an octave");
    // the strengths stay ordered: at every distance Low <= Medium <= High
    {
        int bad = 0, i;
        for (i = -60; i <= 70; ++i) {
            const float r = DofGridR(i) * 1.0001f;
            int q[DOFS_N];
            for (s = 0; s < DOFS_N; ++s) q[s] = WantCode(s, DOF_FAR, 1000.0f, 1000.0f * r) + WantCode(s, DOF_NEAR, 1000.0f, 1000.0f * r);
            if (q[0] > q[1] || q[1] > q[2]) ++bad;
        }
        CHECK(!bad, "Low <= Medium <= High at every distance (%d misses)", bad);
    }
}

static void TestPlan(void)
{
    static const float kFoc[] = { 200.0f, 450.0f, 1000.0f, 1800.0f, 3000.0f, 7000.0f, 12000.0f, 20000.0f };
    const float p22 = P22(), p32 = P32();
    int s, f, i, bad, sd;
    for (s = 0; s < DOFS_N; ++s)
        for (f = 0; f < (int)(sizeof kFoc / sizeof kFoc[0]); ++f) {
            DofPlanT pl;
            const float F = kFoc[f];
            const int ok = DofMakePlan(F, p22, p32, s, &pl);
            CHECK(ok && pl.n > 0 && pl.n == pl.side[0].n + pl.side[1].n, "a plan for F %.0f %s", F, kDofStrName[s]);
            for (sd = 0; sd < DOF_SIDES; ++sd) {
                const DofSideT *t = &pl.side[sd];
                CHECK(t->n <= DofRampN(s, sd) && (t->codes & 1u), "side size");
                for (i = 0; i < t->n; ++i) {
                    CHECK(t->s[i].z > 0.0f && t->s[i].z < 1.0f, "slice z inside (0, 1)");
                    CHECK(fabsf(t->s[i].z - (p22 + p32 / t->s[i].w)) < 1e-6f, "z = P22 + P32 / w");
                    CHECK(t->s[i].code >= 1 && t->s[i].code <= DofRampN(s, sd) && (t->codes >> t->s[i].code & 1u), "codes");
                    CHECK(t->s[i].g == kDofRamp[s][sd].g[t->s[i].code - 1], "grid");
                    if (i) CHECK(sd == DOF_FAR ? t->s[i].w > t->s[i - 1].w : t->s[i].w < t->s[i - 1].w, "drawing order (outwards)");
                    if (i) CHECK(t->s[i].code > t->s[i - 1].code, "codes rise outwards");
                }
            }
            // every view depth from the near plane to beyond the far plane: the emulated stencil codes give the code the
            // depth asks for (beyond the far plane: the deepest kept far slice)
            bad = 0;
            for (i = 0; i <= 800; ++i) {
                const float w = kN * 1.001f * powf(kF * 1.5f / kN, (float)i / 800.0f);
                const float zs = w >= kF ? 0.0f : p22 + p32 / w;  // beyond the far plane: the cleared 0 (the sky)
                for (sd = 0; sd < DOF_SIDES; ++sd) {
                    const int got = DofEmulateCode(&pl.side[sd], sd, zs);
                    int want = WantCode(s, sd, F, w);
                    if (sd == DOF_FAR && (w >= kF || (want && DofSliceW(F, s, sd, want) >= kF))) {  // a far slice past the far plane: dropped
                        int c, deep = 0;
                        for (c = 0; c < pl.side[sd].n; ++c)
                            if (w >= kF || pl.side[sd].s[c].w < w) deep = pl.side[sd].s[c].code;
                        want = deep;
                    }
                    if (got != want) ++bad;
                }
            }
            CHECK(!bad, "the marking gives the asked code everywhere (F %.0f %s: %d misses)", F, kDofStrName[s], bad);
            CHECK(pl.maxLevel >= 1 && pl.maxLevel <= DOF_LEVELS && (pl.levels & 1u) == 0, "levels");
        }
    {   // Medium at 1000: far 8 slices from 1414 (2^(12/24)), near 4 from 561 (2^(-20/24)); half and quarter size
        DofPlanT pl;
        DofMakePlan(1000.0f, p22, p32, DOFS_MEDIUM, &pl);
        CHECK(pl.side[DOF_FAR].n == 8 && pl.side[DOF_NEAR].n == 4 && pl.n == 12, "Medium's slices (%d + %d)", pl.side[0].n, pl.side[1].n);
        CHECK(fabsf(pl.side[DOF_FAR].s[0].w - 1414.21f) < 0.1f && fabsf(pl.side[DOF_NEAR].s[0].w - 561.23f) < 0.1f, "Medium's first depths (%.2f %.2f)",
              pl.side[DOF_FAR].s[0].w, pl.side[DOF_NEAR].s[0].w);
        CHECK(pl.maxLevel == 2 && pl.levels == 6u, "Medium uses the half and quarter levels");
        DofMakePlan(1000.0f, p22, p32, DOFS_LOW, &pl);
        CHECK(pl.side[DOF_FAR].n == 4 && pl.side[DOF_NEAR].n == 4 && pl.maxLevel == 1 && pl.levels == 2u, "Low: half size only");
        DofMakePlan(1000.0f, p22, p32, DOFS_HIGH, &pl);
        CHECK(pl.n == 20 && pl.maxLevel == 3 && pl.levels == 14u, "High: 12 + 8 slices, all three levels");
    }
    {   // a far focus: slices past the far plane are dropped, near ones remain
        DofPlanT pl;
        DofMakePlan(20000.0f, p22, p32, DOFS_HIGH, &pl);
        for (sd = 0; sd < DOF_SIDES; ++sd)
            for (i = 0; i < pl.side[sd].n; ++i) CHECK(pl.side[sd].s[i].w < kF, "no slice past the far plane");
        CHECK(pl.side[DOF_FAR].n == 8 && pl.side[DOF_NEAR].n == 8, "F 20000 High: 8 far (up to 2^(20/24)) + 8 near (%d %d)", pl.side[0].n,
              pl.side[1].n);
        DofMakePlan(20000.0f, p22, p32, DOFS_LOW, &pl);
        CHECK(pl.side[DOF_FAR].n == 1 && pl.side[DOF_NEAR].n == 4, "F 20000 Low: 1 far + 4 near");
    }
    {
        DofPlanT pl;
        CHECK(!DofMakePlan(0.0f, p22, p32, DOFS_LOW, &pl) && !DofMakePlan(-5.0f, p22, p32, DOFS_LOW, &pl), "no focus, no plan");
        CHECK(!DofMakePlan(1000.0f, 0.0f, p32, DOFS_LOW, &pl) && !DofMakePlan(1000.0f, p22, 0.0f, DOFS_LOW, &pl) &&
              !DofMakePlan(1000.0f, 0.5f, p32, DOFS_LOW, &pl) && !DofMakePlan(1000.0f, sqrtf(-1.0f), p32, DOFS_LOW, &pl),
              "a bad projection: no plan");
        CHECK(!DofMakePlan(1000.0f, p22, p32, DOFS_N, &pl) && !DofMakePlan(1000.0f, p22, p32, -1, &pl) && pl.n == 0, "bad strength");
        CHECK(DofProjOk(p22, p32) && !DofProjOk(-1.5f, p32), "proj guard");
        DofPlanCodes(DOFS_N, &pl);
        CHECK(pl.n == 0 && pl.side[0].codes == 0, "no codes for a bad strength");
    }
}

// v3.47 DF347: the user's request: no hard start; a gradual ramp from sharp to the first level and between levels, near and far
static void TestEase(void)
{
    const float p22 = P22(), p32 = P32(), F = 1000.0f;
    int s, sd, blend;
    for (s = 0; s < DOFS_N; ++s)
        for (blend = 1; blend >= 0; --blend) {
            DofPlanT pl;
            DofMakePlan(F, p22, p32, s, &pl);
            for (sd = 0; sd < DOF_SIDES; ++sd) {
                int i, mono = 1, first = -1, L;
                float prev = 0.0f, maxStep = 0.0f, area = 0.0f, area346 = 0.0f, cross[DOF_LEVELS * DOF_Q + 1] = { 0 };
                const int N = 3000;
                for (i = 0; i <= N; ++i) {  // outwards from the focus: far 1 .. 8 x F, near 1 .. 1 / 4 x F
                    const float oct = 3.0f * (float)i / (float)N, r = sd == DOF_FAR ? powf(2.0f, oct) : powf(2.0f, -oct * (2.0f / 3.0f));
                    const float w = F * r, zs = p22 + p32 / w;
                    const int q = DofCodeQ(s, sd, DofEmulateCode(&pl.side[sd], sd, zs));
                    const int lo = DofLevelLo(q, blend);
                    const float b = blend ? (float)q / (float)DOF_Q : (float)lo;
                    const float d = b - prev;
                    if (d < 0.0f) mono = 0;
                    if (d > maxStep) maxStep = d;
                    if (first < 0 && b > 0.0f) {
                        first = i;
                        CHECK(b <= (blend ? 0.25f : 1.0f) + 1e-6f, "%s %s blend %d: the first blur %.2f", kDofStrName[s], kDofSideName[sd], blend, b);
                    }
                    for (L = 1; L <= DOF_LEVELS * DOF_Q; ++L)  // where the blur first reaches each quarter
                        if (cross[L] == 0.0f && b >= (float)L / (float)DOF_Q - 1e-6f) cross[L] = r;
                    if (i) {
                        const float dOct = 3.0f / (float)N * (sd == DOF_FAR ? 1.0f : 2.0f / 3.0f);
                        area += b * dOct, area346 += (float)V346Level(s, r) * dOct;
                    }
                    prev = b;
                }
                CHECK(mono, "%s %s blend %d: the blur never falls going away from the focus", kDofStrName[s], kDofSideName[sd], blend);
                CHECK(maxStep <= (blend ? 0.25f : 1.0f) + 1e-6f, "%s %s blend %d: no step larger than %s (%.2f)", kDofStrName[s],
                      kDofSideName[sd], blend, blend ? "a quarter level" : "a level", maxStep);
                CHECK(first > 0, "%s %s: sharp at the focus", kDofStrName[s], kDofSideName[sd]);
                for (L = 1; L <= DOF_LEVELS; ++L) {  // each level's ramp centred within 0.1 octave of v3.46's step
                    const float old = V346Step(s, sd, L);
                    float c = 0.0f;
                    int k, have = 1;
                    if (old <= 0.0f) {
                        CHECK(cross[L * DOF_Q] == 0.0f, "%s %s: no level %d (as v3.46)", kDofStrName[s], kDofSideName[sd], L);
                        continue;
                    }
                    if (blend) {  // the mean (log distance) of its four quarter steps
                        for (k = 1; k <= DOF_Q; ++k) {
                            const float r = cross[(L - 1) * DOF_Q + k];
                            if (!(r > 0.0f)) have = 0;
                            else c += log2f(r) / (float)DOF_Q;
                        }
                    } else {      // the one whole step
                        have = cross[L * DOF_Q] > 0.0f;
                        if (have) c = log2f(cross[L * DOF_Q]);
                    }
                    CHECK(have && fabsf(c - log2f(old)) <= 0.1f, "%s %s blend %d level %d: centred on v3.46's step %.2f (%.3f)", kDofStrName[s],
                          kDofSideName[sd], blend, L, old, have ? powf(2.0f, c) : 0.0f);
                }
                // (without the blend shader, a fallback, each whole step sits at the ramp's second quarter: a little more blur)
                CHECK(fabsf(area - area346) <= (blend ? 0.12f : 0.16f), "%s %s blend %d: about v3.46's blur in all (%.3f vs %.3f level octaves)",
                      kDofStrName[s], kDofSideName[sd], blend, area, area346);
            }
        }
    {   // v3.46's hard start, for the record: Medium went from sharp to a whole level at once at F x 1.6
        CHECK(V346Level(DOFS_MEDIUM, 1.59f) == 0 && V346Level(DOFS_MEDIUM, 1.61f) == 1, "v3.46: Medium's hard start at 1.6");
    }
}

// v3.47 DF347: the composite's draws give every code its blend of two levels
static void EmulateOps(const DofOp *op, int n, int code, float wt[DOF_LEVELS + 1])
{
    int i, L;
    for (L = 0; L <= DOF_LEVELS; ++L) wt[L] = L == 0 ? 1.0f : 0.0f;
    for (i = 0; i < n; ++i) {
        const int pass = op[i].func == DOFC_FROM ? code >= op[i].code : code == op[i].code;  // (ref >= 1: code 0 never passes)
        if (!pass) continue;
        for (L = 0; L <= DOF_LEVELS; ++L) wt[L] = (1.0f - op[i].a) * wt[L] + op[i].a * (L == op[i].level ? 1.0f : 0.0f);
    }
}
static void TestCompose(void)
{
    int s, sd, blend, drop;
    for (s = 0; s < DOFS_N; ++s)
        for (sd = 0; sd < DOF_SIDES; ++sd)
            for (blend = 0; blend < 2; ++blend)
                for (drop = 0; drop < 3; ++drop) {  // all codes; the outer half dropped (past the far plane); every other one
                    DofPlanT pl;
                    DofOp op[DOF_OPS_MAX];
                    DofSideT *t;
                    int n, c, i, bad = 0, seenBlend = 0, order = 1;
                    const int N = DofRampN(s, sd);
                    DofPlanCodes(s, &pl);
                    t = &pl.side[sd];
                    if (drop == 1) for (c = N / 2 + 1; c <= N; ++c) t->codes &= ~(1u << c);
                    if (drop == 2) for (c = 2; c <= N; c += 2) t->codes &= ~(1u << c);
                    n = DofComposeOps(t, s, sd, blend, op);
                    CHECK(n > 0 && n <= DOF_OPS_MAX, "ops %d", n);
                    for (i = 0; i < n; ++i) {
                        CHECK(op[i].level >= 1 && op[i].level <= DOF_LEVELS && op[i].code >= 1 && op[i].code <= N && (t->codes >> op[i].code & 1u),
                              "op %d sane", i);
                        CHECK(op[i].a > 0.0f && op[i].a <= 1.0f, "op weight");
                        if (op[i].a < 1.0f) {
                            seenBlend = 1;
                            CHECK(blend && op[i].func == DOFC_EQUAL, "a blend draw is per code, only with the shader");
                        } else if (seenBlend) {
                            order = 0;
                        }
                    }
                    CHECK(order, "%s %s: the opaque draws come first (DofCompose switches the states once)", kDofStrName[s], kDofSideName[sd]);
                    for (c = 0; c <= N; ++c) {
                        float wt[DOF_LEVELS + 1], ex[DOF_LEVELS + 1] = { 0 }, mean = 0.0f;
                        int L;
                        const int q = c && (t->codes >> c & 1u) ? DofCodeQ(s, sd, c) : 0;
                        if (c && !(t->codes >> c & 1u)) continue;
                        EmulateOps(op, n, c, wt);
                        if (blend) {
                            const int lo = q / DOF_Q;
                            const float fr = (float)(q % DOF_Q) / (float)DOF_Q;
                            ex[lo] = 1.0f - fr;
                            if (fr > 0.0f) ex[lo + 1] = fr;
                        } else {
                            ex[DofLevelLo(q, 0)] = 1.0f;
                        }
                        for (L = 0; L <= DOF_LEVELS; ++L) {
                            if (fabsf(wt[L] - ex[L]) > 1e-5f) ++bad;
                            mean += (float)L * wt[L];
                        }
                        if (blend && fabsf(mean - (float)q / (float)DOF_Q) > 1e-5f) ++bad;
                    }
                    CHECK(!bad, "%s %s blend %d drop %d: every code gets lerp(level q / 4, the next, q %% 4 / 4) (%d misses)", kDofStrName[s],
                          kDofSideName[sd], blend, drop, bad);
                }
}

// v3.47 DF347: the pause's depth map (a strength-free index) marks every strength's codes as the depth itself would
static int EmulateBand(const DofSliceList *bl, float storedZ)  // DofBandDraw: far-type slices near to far, alpha = code x 8
{
    int i;
    unsigned a = 0;
    for (i = 0; i < bl->n; ++i)
        if (bl->s[i].z > storedZ) a = DofBandAlpha(bl->s[i].code);
    return (int)a;
}
static void TestBand(void)
{
    int b[DOF_BAND_MAX], n = DofBandGrid(b), i, s, sd, c, cam;
    CHECK(n > 0 && n <= DOF_BAND_MAX && DofBandAlpha(n) <= 255u, "the band grid fits the map's alpha (%d)", n);
    for (i = 1; i < n; ++i) CHECK(b[i] > b[i - 1], "sorted, distinct");
    for (s = 0; s < DOFS_N; ++s)
        for (sd = 0; sd < DOF_SIDES; ++sd)
            for (c = 0; c < DofRampN(s, sd); ++c) CHECK(DofBandIndex(kDofRamp[s][sd].g[c]) >= 0, "every slice is in the band grid");
    CHECK(DofBandIndex(1000) == -1 && DofBandIndex(0) == -1, "not in it");
    {
        unsigned ref, a;
        int k, bad = 0;
        CHECK(DofBandTest(DOF_FAR, 1000, &ref) == 0, "a slice not in the grid: no test");
        // a backend that rounds the sampled alpha by up to 3 levels marks the same codes (the references are half-way)
        for (s = 0; s < DOFS_N; ++s)
            for (sd = 0; sd < DOF_SIDES; ++sd) {
                DofPlanT pl;
                DofPlanCodes(s, &pl);
                for (k = 0; k <= n; ++k)
                    for (a = DofBandAlpha(k) > 3u ? DofBandAlpha(k) - 3u : 0u; a <= DofBandAlpha(k) + 3u; ++a)
                        if (DofBandEmulate(&pl.side[sd], sd, a) != DofBandEmulate(&pl.side[sd], sd, DofBandAlpha(k))) ++bad;
                for (c = 0; c < pl.side[sd].n; ++c) {
                    const int f = DofBandTest(sd, pl.side[sd].s[c].g, &ref);
                    CHECK(f && ref % DOF_BAND_UNIT == DOF_BAND_UNIT / 2u && ref < 255u, "a half-way reference (%u)", ref);
                }
            }
        CHECK(!bad, "the map's marking is the same for an alpha rounded by 3 levels (%d misses)", bad);
    }
    for (cam = 0; cam < 2; ++cam) {  // the test camera; one with a near plane of 100 (a near slice in front of it)
        const float n0 = cam ? 100.0f : kN, f0 = cam ? 30000.0f : kF;
        const float p22 = -n0 / (f0 - n0), p32 = f0 * n0 / (f0 - n0);
        static const float kFoc[] = { 200.0f, 1000.0f, 5000.0f, 20000.0f };
        int f;
        for (f = 0; f < 4; ++f) {
            DofSliceList bl;
            const float F = kFoc[f];
            CHECK(DofMakeBandList(F, p22, p32, &bl) && bl.n <= n, "a band list");
            for (i = 1; i < bl.n; ++i) CHECK(bl.s[i].w > bl.s[i - 1].w && bl.s[i].code > bl.s[i - 1].code, "near to far");
            for (s = 0; s < DOFS_N; ++s) {
                DofPlanT real, codes;
                int bad = 0, k;
                DofMakePlan(F, p22, p32, s, &real);
                DofPlanCodes(s, &codes);
                for (k = 0; k <= 1200; ++k) {
                    const float w = n0 * 1.001f * powf(f0 * 1.5f / n0, (float)k / 1200.0f);
                    const float zs = w >= f0 ? 0.0f : p22 + p32 / w;
                    const unsigned a = (unsigned)EmulateBand(&bl, zs);
                    for (sd = 0; sd < DOF_SIDES; ++sd)
                        if (DofBandEmulate(&codes.side[sd], sd, a) != DofEmulateCode(&real.side[sd], sd, zs)) ++bad;
                }
                CHECK(!bad, "cam %d F %.0f %s: the map marks the codes the depth marks (%d misses)", cam, F, kDofStrName[s], bad);
            }
        }
    }
    {   // the card: far at the top, in focus in the lower middle (every strength), near at the bottom; the index never rises downwards
        int prev = 1 << 20, mono = 1, k;
        float y;
        for (k = 0; k <= 200; ++k) {
            const int v = DofCardBand((float)k / 200.0f);
            if (v > prev) mono = 0;
            prev = v;
        }
        CHECK(mono && DofCardBand(0.0f) == n && DofCardBand(1.0f) == 0, "the card: all far at the top, all near at the bottom");
        for (s = 0; s < DOFS_N; ++s) {
            DofPlanT pl;
            float y0 = -1.0f, y1 = -1.0f;
            DofPlanCodes(s, &pl);
            for (y = 0.0f; y <= 1.0f; y += 0.005f) {
                const unsigned a = DofBandAlpha(DofCardBand(y));
                if (!DofBandEmulate(&pl.side[DOF_FAR], DOF_FAR, a) && !DofBandEmulate(&pl.side[DOF_NEAR], DOF_NEAR, a)) {
                    if (y0 < 0.0f) y0 = y;
                    y1 = y;
                }
            }
            CHECK(y0 > 0.3f && y0 < 0.6f && y1 > 0.7f && y1 < 0.9f, "%s: the card is sharp in the lower middle (%.3f .. %.3f)", kDofStrName[s], y0, y1);
        }
    }
}

// v3.47 DF347: the cost: draws a frame (the copy and the chain's 4 passes apart), against v3.46 (marks where the level
// changed, one composite draw a blurred code, two clears)
static void TestCost(void)
{
    static const int kV346[DOFS_N] = { 6, 8, 12 };  // Low: 2 marks + 2 composites + 2 clears; Medium 3 + 3 + 2; High 5 + 5 + 2
    const float p22 = P22(), p32 = P32();
    int s;
    for (s = 0; s < DOFS_N; ++s) {
        DofPlanT pl;
        int comp = 0, comp0 = 0, n, n0;
        DofMakePlan(1000.0f, p22, p32, s, &pl);
        n = DofFrameDraws(&pl, s, 1, &comp);
        n0 = DofFrameDraws(&pl, s, 0, &comp0);
        printf("  cost %s: %d draws a frame (%d slices, %d composite, 3 clears; without the blend shader %d), v3.46 %d\n", kDofStrName[s], n,
               pl.n, comp, n0, kV346[s]);
        CHECK(n == pl.n + comp + 3 && n <= 45 && n0 < n, "%s: %d draws", kDofStrName[s], n);
    }
}

static void TestFocus(void)
{
    float f = 0.0f;
    int i;
    CHECK(DofFocusStep(0.0f, 1000.0f, 0.016f, 0) == 1000.0f, "first value snaps");
    CHECK(DofFocusStep(1000.0f, 50.0f, 0.016f, 0) == DOF_F_MIN, "clamped and snapped (a 3x jump)");
    CHECK(DofFocusStep(1000.0f, 1e7f, 0.016f, 0) == DOF_F_MAX, "clamped high");
    f = 1000.0f;
    for (i = 0; i < 60; ++i) f = DofFocusStep(f, 2000.0f, 1.0f / 60.0f, 0);
    CHECK(f > 1900.0f && f < 2000.0f, "1 s at 60 fps: close to the target (%.1f)", f);
    f = 1000.0f;
    for (i = 0; i < 30; ++i) f = DofFocusStep(f, 2000.0f, 1.0f / 30.0f, 0);
    {
        float g = 1000.0f;
        for (i = 0; i < 60; ++i) g = DofFocusStep(g, 2000.0f, 1.0f / 60.0f, 0);
        CHECK(fabsf(f - g) < 1.0f, "the same at 30 and 60 fps (%.2f / %.2f)", f, g);
    }
    f = DofFocusStep(1000.0f, 1200.0f, 0.0f, 0);
    CHECK(f == 1000.0f, "dt 0: no move");
    CHECK(DofFocusStep(1000.0f, 1200.0f, 0.016f, 1) == 1200.0f, "snap");
    CHECK(DofFocusStep(1000.0f, 1200.0f, 5.0f, 0) > 1190.0f, "a long frame: almost there");
}

static void TestGate(void)
{
    CHECK(DofGate(DOF_OFF, 1, 0, 1, 0) == DOFG_OFF, "off");
    CHECK(DofGate(DOF_ALWAYS, 0, 0, 1, 0) == DOFG_NOTPLAY, "not play");
    CHECK(DofGate(DOF_ALWAYS, 1, 2, 1, 0) == DOFG_PHASE && DofGate(DOF_ALWAYS, 1, 3, 1, 0) == DOFG_PHASE, "paused");
    CHECK(DofGate(DOF_CUT, 1, 0, 1, 0) == DOFG_GAMEPLAY && DofGate(DOF_CUT, 1, 1, 1, 0) == DOFG_ON, "Cut-scenes");
    CHECK(DofGate(DOF_ALWAYS, 1, 0, 1, 0) == DOFG_ON && DofGate(DOF_ALWAYS, 1, 1, 1, 0) == DOFG_ON, "Always");
    CHECK(DofGate(DOF_ALWAYS, 1, 0, 0, 0) == DOFG_VIEW, "a peephole view");
    CHECK(DofGate(DOF_ALWAYS, 1, 0, 1, 1) == DOFG_FP && DofGate(DOF_ALWAYS, 1, 1, 1, 1) == DOFG_ON, "Room 302: play off, cut-scenes on");
    CHECK(DofIsRoom302(1) && DofIsRoom302(0x0A) && !DofIsRoom302(2) && !DofIsRoom302(0), "Room 302 stages");
    CHECK(DofGate(7, 1, 0, 1, 0) == DOFG_OFF && DofGate(-1, 1, 0, 1, 0) == DOFG_OFF, "bad mode");
}

static void TestPick(void)
{
    float F = -1.0f;
    CHECK(DofPickFocus(1, 1, 2500.0f, 1, 900.0f, &F) == DOFF_SCENE && F == 2500.0f, "a cut-scene's own focus wins");
    CHECK(DofPickFocus(0, 1, 2500.0f, 1, 900.0f, &F) == DOFF_PLAYER && F == 900.0f, "in play the player (a stale scene focus)");
    CHECK(DofPickFocus(1, 0, 2500.0f, 1, 900.0f, &F) == DOFF_PLAYER && F == 900.0f, "a cut-scene without its own focus");
    CHECK(DofPickFocus(1, 1, 0.0f, 1, 900.0f, &F) == DOFF_PLAYER, "a zero scene focus");
    CHECK(DofPickFocus(0, 0, 0.0f, 1, 60.0f, &F) == DOFF_NONE && F == 0.0f, "first person: none");
    CHECK(DofPickFocus(0, 0, 0.0f, 0, 900.0f, &F) == DOFF_NONE, "no player");
    CHECK(DofPickFocus(0, 0, 0.0f, 1, sqrtf(-1.0f), &F) == DOFF_NONE, "NaN");
}

static void TestProbe(void)
{
    DofProbeIn in;
    float share;
    int c;
    CHECK(DofProbeCodeOf(100.0f) == 0 && DofProbeCodeOf(160.0f) == 1 && DofProbeCodeOf(1000.0f) == 3 && DofProbeCodeOf(1e9f) == DOF_PROBE_N,
          "probe codes");
    // the light grid's 2D depth (view depth ~27 000) or a cleared buffer: flat
    memset(&in, 0, sizeof in);
    in.n = DOF_PROBE_GX * DOF_PROBE_GY;
    in.hist[DofProbeCodeOf(27000.0f)] = in.n - 5, in.hist[3] = 5;
    in.figMin = -1;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_FLAT && share > 0.99f, "overwritten: flat");
    // a room: spread codes, her figure at code 3 (F 1500: expected 3)
    memset(&in, 0, sizeof in);
    in.n = 576;
    for (c = 0; c <= DOF_PROBE_N; ++c) in.hist[c] = 64;
    in.figMin = 3, in.figN = 9;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_OK, "a room's depth: ok");
    in.figMin = 4;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_OK, "one code of slack (the figure's edge)");
    in.figMin = 6;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_PLAYER, "only far things where she stands");
    in.figN = 0;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_OK, "no figure samples: the grid decides");
    in.n = 20;
    CHECK(DofProbeVerdict(&in, 1500.0f, &share) == DOFP_FEW, "too few");
    {   // v3.45 QS345-02 / QS345-03: the fallback reads with the legacy meaning; the write order per value
        int rw, mk;
        CHECK(DofStrMigrate("Low", 0, &rw, &mk) == DOFS_MEDIUM && DofStrMigrate("1", 0, &rw, &mk) == DOFS_HIGH &&
              DofStrMigrate("0", 0, &rw, &mk) == DOFS_MEDIUM && DofStrMigrate("High", 0, &rw, &mk) == DOFS_HIGH &&
              DofStrMigrate("Medium", 0, &rw, &mk) == DOFS_MEDIUM, "v3.45 QS345-02: with no marker (the ini fallback too) a legacy "
              "Low / 0 reads Medium, 1 / High reads High");
        CHECK(DofStrWriteOrder(DOFS_LOW, 1) == DOFW_VALUE && DofStrWriteOrder(DOFS_MEDIUM, 1) == DOFW_VALUE &&
              DofStrWriteOrder(DOFS_LOW, 0) == DOFW_MARK_VALUE && DofStrWriteOrder(DOFS_MEDIUM, 0) == DOFW_VALUE_MARK &&
              DofStrWriteOrder(DOFS_HIGH, 0) == DOFW_VALUE_MARK, "v3.45 QS345-03: marked: the value only; unmarked: Medium / High "
              "first (same meaning either way), a new Low after the marker (taken back if the value fails)");
    }
    CHECK(DofProbeRetry(DOFP_NOREAD, 0, 1) && DofProbeRetry(DOFP_NOREAD, 0, 2) && !DofProbeRetry(DOFP_NOREAD, 0, DOF_PROBE_TRIES) &&
          !DofProbeRetry(DOFP_NOREAD, 1, 1) && !DofProbeRetry(DOFP_OK, 0, 1) && !DofProbeRetry(DOFP_NONE, 0, 1) &&
          !DofProbeRetry(DOFP_FLAT, 0, 1), "v3.45 RB345-01: a transient no-read is retried (tries left, read back not off); "
          "the terminal off and every verdict end the room's probe");
    CHECK(DOFP_NOREAD < DOFP_N && strstr(kDofProbeName[DOFP_NOREAD], "not probed: no read back") == kDofProbeName[DOFP_NOREAD] &&
          strstr(kDofProbeName[DOFP_NOREAD], ": on"), "v3.45 RB345: the honest verdict: not probed, drawn");
}

static void TestPreview(void)
{
    DofPrevIn in;
    int why;
    memset(&in, 0, sizeof in);
    in.previewOn = 1, in.hook = 1, in.pausedGame = 1, in.haveCopy = 1, in.haveDepth = 1;
    CHECK(DofPreviewMode(&in, &why) == DOFV_SCENE && why == DOFW_SCENE, "the paused scene");
    in.haveDepth = 0;
    CHECK(DofPreviewMode(&in, &why) == DOFV_CARD && why == DOFW_NODEPTH, "no depth map: the card");
    in.haveDepth = 1, in.haveCopy = 0;
    CHECK(DofPreviewMode(&in, &why) == DOFV_CARD && why == DOFW_NOCOPY, "no copy");
    in.haveCopy = 1, in.pausedGame = 0;
    CHECK(DofPreviewMode(&in, &why) == DOFV_CARD && why == DOFW_NOPAUSE, "from the title");
    in.failed = 1;
    CHECK(DofPreviewMode(&in, &why) == DOFV_NONE && why == DOFW_FAILED, "failed");
    in.failed = 0, in.hook = 0;
    CHECK(DofPreviewMode(&in, &why) == DOFV_NONE && why == DOFW_NOHOOK, "no hook");
    in.previewOn = 0;
    CHECK(DofPreviewMode(&in, &why) == DOFV_NONE && why == DOFW_OFF, "off");
    // (v3.47 DF347: the card's sample depth is tested in TestBand)
    CHECK(DofCodeAlpha(8) == 128u && DofCodeAlpha(0) == 0u && DofCodeAlpha(-3) == 0u, "the probe's code alpha");
}

// DF343-001: the cleanup runs on every exit after the first stencil write (success, a failed draw, a fault), once
static int RunFrame(DofStGuard *g, int failAt, int faultAt)  // steps 0 clean, 1 mark, 2 compose; 1 = all drawn
{
    int step, ok = 1;
    DofStBegin(g);
    for (step = 0; step < 3 && ok; ++step) {
        if (step == faultAt) return -1;  // the fault handler cleans (below)
        ok = step != failAt;
    }
    if (DofStEnd(g)) { /* the cleanup quad */ }
    return ok;
}
static void TestStencilGuard(void)
{
    int f, k;
    for (f = -1; f < 3; ++f)
        for (k = -1; k < 3; ++k) {
            DofStGuard g;
            int r;
            memset(&g, 0, sizeof g);
            r = RunFrame(&g, f, k);
            if (r < 0) CHECK(DofStEnd(&g) == 1, "a fault at step %d: the handler cleans", k);
            CHECK(!g.dirty && g.cleaned == 1 && g.begun == 1, "fail %d fault %d: cleaned exactly once (%d)", f, k, g.cleaned);
            CHECK(DofStEnd(&g) == 0, "a second End does nothing");
        }
    {
        DofStGuard g;
        memset(&g, 0, sizeof g);
        CHECK(DofStEnd(&g) == 0, "no write begun: no cleanup (the chain failed before marking)");
    }
}

// DF343-001b: every exit (normal, a failed step, a fault at any point after the save) restores once, in order
static void TestRestorePlan(void)
{
    int saved, rs, held, dirty;
    for (saved = 0; saved < 2; ++saved)
        for (rs = 0; rs < 2; ++rs)
            for (held = 0; held < 2; ++held)
                for (dirty = 0; dirty < 2; ++dirty) {
                    DofRestoreIn f;
                    int st[DOFR_N], n, i, pos[DOFR_N];
                    f.saved = saved, f.rsSaved = rs, f.held = held, f.dirty = dirty;
                    n = DofRestorePlan(&f, st);
                    for (i = 0; i < DOFR_N; ++i) pos[i] = -1;
                    for (i = 0; i < n; ++i) pos[st[i]] = i;
                    CHECK((pos[DOFR_TARGET] >= 0) == held && (pos[DOFR_RELEASE] >= 0) == held, "target set back / released when held");
                    CHECK((pos[DOFR_CLEAN] >= 0) == (held && dirty), "the stencil cleaned when written, on the saved target");
                    CHECK((pos[DOFR_STATE] >= 0) == (saved || rs) && (pos[DOFR_VIEWPORT] >= 0) == saved, "states and viewport");
                    if (pos[DOFR_CLEAN] >= 0) CHECK(pos[DOFR_TARGET] < pos[DOFR_CLEAN] && (pos[DOFR_STATE] < 0 || pos[DOFR_CLEAN] < pos[DOFR_STATE]),
                                                    "clean after the target is back, before the states");
                    if (pos[DOFR_STATE] >= 0 && pos[DOFR_TARGET] >= 0) CHECK(pos[DOFR_TARGET] < pos[DOFR_STATE], "target before states");
                    if (pos[DOFR_VIEWPORT] >= 0) CHECK(pos[DOFR_STATE] < pos[DOFR_VIEWPORT], "viewport after FxRestore (it sets its own)");
                    if (pos[DOFR_RELEASE] >= 0) CHECK(pos[DOFR_RELEASE] == n - 1, "released last");
                    CHECK(DofRestorePlan(&f, st) == 0, "restore once: a second exit (a fault in the handler path) does nothing");
                }
}

// v3.44 DF344: every asked frame gets one reason; the room line's lists; the first of each kind
static void TestWhyNot(void)
{
    DofTally t;
    char buf[1024];
    int g, k, first = 0;
    CHECK(DofGateWhyNot(DOFG_ON) == DOFN_DRAWN && DofGateWhyNot(DOFG_OFF) < 0 && DofGateWhyNot(DOFG_NOTPLAY) < 0, "gate: on / not counted");
    for (g = DOFG_PHASE; g < DOFG_N; ++g) {
        const int w = DofGateWhyNot(g);
        CHECK(w >= DOFN_PHASE && w <= DOFN_DESIGN_END, "gate %d is a by-design skip (%d)", g, w);
    }
    CHECK(DofGateWhyNot(DOFG_FP) == DOFN_FP && DofGateWhyNot(DOFG_GAMEPLAY) == DOFN_CUTMODE && DofGateWhyNot(DOFG_VIEW) == DOFN_VIEW,
          "gate names");
    // the no-focus reasons (v3.43: a fault in the matrix call, every frame: READ)
    CHECK(DofNoFocusWhy(0, 1500.0f, 1, 0) == DOFN_READ, "a fault reading the player / the matrix");
    CHECK(DofNoFocusWhy(1, -300.0f, 0, 0) == DOFN_BEHIND && DofNoFocusWhy(1, 0.0f, 0, 0) == DOFN_BEHIND, "behind the camera");
    CHECK(DofNoFocusWhy(1, sqrtf(-1.0f), 0, 0) == DOFN_BEHIND && DofNoFocusWhy(1, 2.0e6f, 1, 0) == DOFN_BEHIND, "NaN / too far");
    CHECK(DofNoFocusWhy(1, 120.0f, 1, 0) == DOFN_NEAR && DofNoFocusWhy(1, DOF_F_NOFOCUS, 1, 1) == DOFN_NEAR, "first person");
    CHECK(DofNoFocusWhy(1, 1500.0f, 0, 1) == DOFN_OFFSCREEN, "a cut-scene with her off screen");
    for (k = 0; k < 2; ++k) {  // whatever DofNoFocusWhy says is a refusal, never "drawn" or a by-design skip
        const float ws[5] = { -1.0f, 0.0f, 100.0f, 1500.0f, 3.0e6f };
        int i;
        for (i = 0; i < 5; ++i) {
            const int w = DofNoFocusWhy(k, ws[i], i & 1, i & 1);
            CHECK(w > DOFN_DESIGN_END && w < DOFN_N, "no-focus reason %d is a refusal", w);
        }
    }
    // the agreement with DofPickFocus: when it finds the player, DofNoFocusWhy is never needed; when it does not, a reason
    {
        const float ws[6] = { -50.0f, 100.0f, 151.0f, 1500.0f, 9.0e5f, 2.0e6f };
        int i, ph, fig;
        for (i = 0; i < 6; ++i)
            for (ph = 0; ph < 2; ++ph)
                for (fig = 0; fig < 2; ++fig) {
                    float F;
                    const int src = DofPickFocus(ph, 0, 0.0f, fig || ph == 0, ws[i], &F);
                    if (src == DOFF_NONE) {
                        const int w = DofNoFocusWhy(1, ws[i], fig, ph);
                        CHECK(w == DOFN_BEHIND || w == DOFN_NEAR || w == DOFN_OFFSCREEN, "w %.0f phase %d fig %d: %d", ws[i], ph, fig, w);
                    } else {
                        CHECK(src == DOFF_PLAYER && ws[i] > DOF_F_NOFOCUS, "the player found at %.0f", ws[i]);
                    }
                }
    }
    // the tally
    memset(&t, 0, sizeof t);
    first += DofTallyAdd(&t, DOFN_DRAWN);
    first += DofTallyAdd(&t, DOFN_DRAWN);
    CHECK(first == 0, "drawn is never a first refusal");
    CHECK(DofTallyAdd(&t, DOFN_READ) == 1 && DofTallyAdd(&t, DOFN_READ) == 0 && DofTallyAdd(&t, DOFN_READ) == 0, "logged once a room");
    CHECK(DofTallyAdd(&t, DOFN_PHASE) == 1 && DofTallyAdd(&t, DOFN_PHASE) == 0, "a skip once too");
    CHECK(DofTallyAdd(&t, -1) == 0 && DofTallyAdd(&t, DOFN_N) == 0, "out of range: ignored");
    CHECK(DofTallyAsked(&t) == 5 && DofTallyDesign(&t) == 2, "asked 2 drawn + 3 refused; 2 skipped (%u %u)", DofTallyAsked(&t), DofTallyDesign(&t));
    DofTallyText(&t, 0, buf, (int)sizeof buf);
    CHECK(!strcmp(buf, "the player's position could not be projected 3"), "refusals text: %s", buf);
    DofTallyText(&t, 1, buf, (int)sizeof buf);
    CHECK(!strcmp(buf, "paused or an overlay 2"), "skips text: %s", buf);
    for (k = 0; k < 1234; ++k) DofTallyAdd(&t, DOFN_PROBEOFF);
    DofTallyAdd(&t, DOFN_NORT);
    DofTallyText(&t, 0, buf, (int)sizeof buf);
    CHECK(!strcmp(buf, "the player's position could not be projected 3, the room's depth probe turned it off 1234, "
                       "our render targets are not ready 1"), "refusals in enum order: %s", buf);
    memset(&t, 0, sizeof t);
    DofTallyText(&t, 0, buf, (int)sizeof buf);
    CHECK(!strcmp(buf, "none"), "empty: none (%s)", buf);
    for (k = 0; k < DOFN_N; ++k) t.n[k] = 4000000000u;
    DofTallyText(&t, 0, buf, 64);
    CHECK(strlen(buf) == 63, "a small buffer is cut, terminated (%u)", (unsigned)strlen(buf));
    DofTallyText(&t, 0, buf, (int)sizeof buf);
    CHECK(strlen(buf) < sizeof buf - 1 && buf[strlen(buf) - 1] == '0', "every refusal fits the room line's buffer (%u)", (unsigned)strlen(buf));
    for (k = 0; k < DOFN_N; ++k) {
        const char *s = kDofWhyNot[k];
        int c = 0;
        for (; *s; ++s) c += *s == ',' || *s == ';';
        CHECK(c == 0, "reason %d has no comma or semicolon (logcheck splits on them)", k);
    }
}

static void TestSizes(void)
{
    CHECK(DofLevelDim(1920, 1) == 960 && DofLevelDim(1920, 3) == 240 && DofLevelDim(1080, 3) == 135 && DofLevelDim(9, 3) == 8,
          "level sizes");
    CHECK(DofChainBytes(1920, 1080) > 1920u * 1080u * 4u && DofChainBytes(1920, 1080) < 1920u * 1080u * 6u, "chain bytes (%u)",
          DofChainBytes(1920, 1080));
}

int main(void)
{
    TestParse();
    TestTables();
    TestPlan();
    TestEase();     // v3.47 DF347
    TestCompose();  // v3.47 DF347
    TestBand();     // v3.47 DF347
    TestCost();     // v3.47 DF347
    TestFocus();
    TestGate();
    TestPick();
    TestProbe();
    TestPreview();
    TestSizes();
    TestStencilGuard();
    TestRestorePlan();
    TestWhyNot();  // v3.44 DF344
    printf("dof_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
