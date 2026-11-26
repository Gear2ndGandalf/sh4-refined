// v3.40 SH340: native test of src\shadows_sim.h (test\build_shadows_test.cmd).
// v3.41 SH341: the one Soft edge (12 px at 720 lines): the plan per resolution (480 ... 2160 lines, 4:3 ... 32:9: the
// width within 3 %, no draw stretching more than 2x, the draw / render-target budgets, the buffers' order), the chain's
// 1D emulation (constant regions stay exact, profiles rise monotonically), a 2D emulation of the exact draws against
// v3.40's Softer (the grid along a slanted edge: "blocky" measured), the darkness (inside = the stock two copies for every
// stage's TEXTUREFACTOR, tinted 9-11, cut-scenes and Playable Eileen's ShadowDarkness), thin shapes (a 6 px bar, a 20 px
// leg, a large body at 1440 lines), old ini values (Softer -> Soft), the hook's call check, the preview's mode, the
// copies' lifetime and the kept targets, the card's figure.
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../src/shadows_sim.h"
#include "px_model.h"  // v3.47 PX347

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void TestParse(void)
{
    int was = -1;
    CHECK(ShParseEdge("Stock", &was) == SH_STOCK && was == 0, "Stock");
    CHECK(ShParseEdge("soft", &was) == SH_SOFT && was == 0, "soft");
    CHECK(ShParseEdge("SOFTER", &was) == SH_SOFT && was == 1, "v3.40's Softer reads as the new Soft (flagged for the log)");
    CHECK(ShParseEdge("Softer", NULL) == SH_SOFT, "Softer, no flag pointer");
    CHECK(ShParseEdge("2", &was) == SH_SOFT && was == 1 && ShParseEdge("0", &was) == SH_STOCK && ShParseEdge("1", &was) == SH_SOFT,
          "digits (2 = v3.40's Softer)");
    CHECK(ShParseEdge("Softest", &was) == -1 && ShParseEdge("", &was) == -1 && ShParseEdge(NULL, &was) == -1 &&
          ShParseEdge("3", &was) == -1 && ShParseEdge("Sof", &was) == -1 && ShParseEdge("Softe", &was) == -1, "not values");
    CHECK(SH_EDGE_N == 2 && !strcmp(kShEdgeName[SH_SOFT], "Soft") && SH_EDGE_IDS == 3, "two values, three id slots kept");
    CHECK(ShClampPx(0.0f, 12.0f) == 12.0f && ShClampPx(-2.0f, 12.0f) == 12.0f && ShClampPx(100.0f, 12.0f) == SH_PX720_MAX &&
          ShClampPx(0.2f, 12.0f) == SH_PX720_MIN && ShClampPx(9.5f, 12.0f) == 9.5f, "clamp");
    CHECK(fabsf(ShTargetPx(SH_SOFT, SH_SOFT_PX720, 720) - 12.0f) < 1e-5f, "720: 12 px (twice v3.40's Softer 6)");
    CHECK(fabsf(ShTargetPx(SH_SOFT, SH_SOFT_PX720, 1440) - 24.0f) < 1e-5f, "1440: 24 px");
    CHECK(fabsf(ShTargetPx(SH_SOFT, SH_SOFT_PX720, 480) - 8.0f) < 1e-5f && fabsf(ShTargetPx(SH_SOFT, SH_SOFT_PX720, 2160) - 36.0f) < 1e-4f, "480 / 2160");
    CHECK(ShTargetPx(SH_STOCK, SH_SOFT_PX720, 1440) == 0.0f, "stock 0");
}

static void TestFilter1D(void)
{
    static float a[SH_STRIP], b[SH_STRIP], p[SH_STRIP];
    float off[SH_KAWASE_MAX];
    int i, n, L, k, ok;
    // v3.40's box (kept): a single texel spreads over two half-res texels (1/4 each) under a 2x pass
    memset(a, 0, sizeof a);
    a[100] = 1.0f;
    n = ShDown1D(a, SH_STRIP, b);
    CHECK(fabsf(b[48]) < 1e-6f && fabsf(b[49] - 0.25f) < 1e-6f && fabsf(b[50] - 0.25f) < 1e-6f && fabsf(b[51]) < 1e-6f,
          "box of 4: %g %g %g %g", b[48], b[49], b[50], b[51]);
    // a constant region (coverage 1: the inside) stays exactly 1 through every chain, and 0 stays 0
    for (L = 1; L <= SH_LEVELS_MAX; ++L)
        for (k = 0; k <= SH_KAWASE_MAX; ++k) {
            ShOffsets(k, 1.3f, off);
            for (i = 0; i < SH_STRIP; ++i) a[i] = 1.0f;
            ShRun1D(L, k, off, a, SH_STRIP, p);
            ok = 1;
            for (i = 0; i < SH_STRIP; ++i) ok &= p[i] == 1.0f;
            CHECK(ok, "inside exact: %d level(s), %d pass(es)", L, k);
            memset(a, 0, sizeof a);
            ShRun1D(L, k, off, a, SH_STRIP, p);
            ok = 1;
            for (i = 0; i < SH_STRIP; ++i) ok &= p[i] == 0.0f;
            CHECK(ok, "outside exact: %d %d", L, k);
            // an edge rises monotonically from 0 to 1 (positive taps only: no ringing, no halo)
            ShEdgeProfile(L, k, off, SH_STRIP / 2 + 3, p);
            ok = p[0] < 1e-6f && fabsf(p[SH_STRIP - 1] - 1.0f) < 1e-6f;
            for (i = 1; i < SH_STRIP; ++i) ok &= p[i] >= p[i - 1] - 1e-6f;
            CHECK(ok, "monotone edge: %d level(s), %d pass(es)", L, k);
        }
    // the width table grows with the last offset (the plan's interpolation needs it) and with the passes / levels
    {
        const clock_t t0 = clock();
        ShPlanInit();
        printf("  the width table (ShPlanInit, at install): %.0f ms\n", 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC);
    }
    CHECK(fabsf(ShOffAt(SH_STEP_HALF) - SH_OFF_LO) < 1e-6f && fabsf(ShOffAt(SH_OFF_STEPS) - SH_OFF_HI) < 1e-6f, "table steps");
    for (L = 1; L <= SH_LEVELS_MAX; ++L) {
        for (k = 1; k <= SH_KAWASE_MAX; ++k) {
            ok = 1;
            for (i = 1; i <= SH_OFF_STEPS; ++i) ok &= g_shWTab[L][k][i] > g_shWTab[L][k][i - 1];
            CHECK(ok, "width table rises with the last offset: L%d n%d (%.2f .. %.2f)", L, k, g_shWTab[L][k][0], g_shWTab[L][k][SH_OFF_STEPS]);
            CHECK(fabsf(g_shWTab[L][k][0] - (k == 1 ? g_shWTab[L][0][0] : g_shWTab[L][k - 1][SH_STEP_HALF])) < 0.01f &&
                  g_shWTab[L][k][SH_OFF_STEPS] > g_shWTab[L][k - 1][SH_OFF_STEPS], "the ranges join up: L%d n%d", L, k);
        }
        if (L > 1) CHECK(g_shWTab[L][0][0] > g_shWTab[L - 1][0][0] * 1.8f, "one more level about doubles the width: L%d", L);
    }
    // the edge's middle (50 %) sits where the stock edge's middle is, before the core curve moves it out (within a pixel)
    // (stock: copy 1 reads the mask at i - 0.5, so its half-way pixel is the edge E; copy 2 at i + 1.5: E - 2; the middle
    // E - 1. The composite's quad from -1 reads at i + 0.5: a symmetric blur crosses 50 % of coverage at E - 1 too)
    for (L = 1; L <= SH_LEVELS_MAX; ++L) {
        const int E = SH_STRIP / 2 + L;
        float x50 = -1.0f, lin[SH_STRIP];
        ShOffsets(2, 0.5f, off);
        ShEdgeProfile(L, 2, off, E, p);
        for (i = 0; i < SH_STRIP; ++i) lin[i] = 1.0f - sqrtf(1.0f - p[i]);  // core^-1: the blurred coverage itself
        for (i = 1; i < SH_STRIP; ++i)
            if (x50 < 0 && lin[i - 1] < 0.5f && lin[i] >= 0.5f) x50 = (float)(i - 1) + (0.5f - lin[i - 1]) / (lin[i] - lin[i - 1]);
        CHECK(fabsf(x50 - (float)(E - 1)) <= 0.75f, "edge middles, %d level(s): stock's %d, the blur's %.2f", L, E - 1, x50);
    }
}

// ---------------------------------------------------------------- plans
static void CheckPlanShape(const ShPlan *p, const char *what)
{
    unsigned written = 1u << SHB_MASK;
    int k, ok = 1;
    CHECK(p->levels >= 1 && p->levels <= SH_LEVELS_MAX && p->kawase >= 0 && p->kawase <= SH_KAWASE_MAX, "%s: range %d %d", what, p->levels, p->kawase);
    CHECK(p->nsteps == 2 * p->levels + p->kawase && p->nsteps <= SH_PASSES_MAX, "%s: %d draws", what, p->nsteps);
    CHECK(ShMaxStretch(p) <= 2.0f + 1e-6f, "%s: a draw stretches %.3fx", what, ShMaxStretch(p));
    for (k = 1; k <= p->levels; ++k) ok &= p->w[k] >= SH_RT_MIN && p->h[k] >= SH_RT_MIN && p->w[k] == ShHalf(p->w[k - 1]);
    CHECK(ok, "%s: level sizes", what);
    for (k = 0; k < p->kawase; ++k) ok &= p->off[k] >= SH_LAST_LO - 1e-6f && p->off[k] <= SH_OFF_HI + 1e-6f && (k == p->kawase - 1 || p->off[k] == SH_OFF_LO);
    CHECK(ok, "%s: offsets", what);
    for (k = 0; k < p->nsteps; ++k) {
        const ShStep *s = &p->step[k];
        ok &= (written & (1u << s->src)) != 0 && s->src != s->dst;  // reads a buffer already drawn, never the one it draws
        if (s->kind != SHS_COMP) written |= 1u << s->dst;
        ok &= (k == 0) == (s->kind == SHS_NORM);                     // the coverage pass first, once
        ok &= (k == p->nsteps - 1) == (s->kind == SHS_COMP);         // the composite last, once
        if (s->kind == SHS_COMP) ok &= s->sw == p->w[1] && s->sh == p->h[1] && s->dw == p->w[0] && s->dh == p->h[0];  // from the half size
        if (s->kind == SHS_DOWN || s->kind == SHS_NORM) ok &= fabsf(s->du * (float)s->sw - 1.0f) < 1e-5f;
        if (s->kind == SHS_UP || s->kind == SHS_COMP) ok &= fabsf(s->du * (float)s->sw - SH_TENT) < 1e-5f;
        if (s->kind == SHS_KAWASE) ok &= s->sw == p->w[p->levels] && s->dw == p->w[p->levels];
        if (s->kind == SHS_UP) ok &= s->dw <= 2 * s->sw && s->dw >= 2 * s->sw - 1;
    }
    CHECK(ok, "%s: the draws' order, buffers and taps", what);
    CHECK(ShChainBytes(p) * 100u <= (unsigned)p->w[0] * (unsigned)p->h[0] * 4u * 55u, "%s: chain %.1f MB within 55 %% of one full-size target",
          what, ShChainBytes(p) / 1048576.0);
}

static void TestPlans(void)
{
    static const int kH[] = { 480, 600, 720, 768, 900, 1050, 1080, 1200, 1440, 1600, 2160 };
    static const int kAsp[][2] = { { 4, 3 }, { 16, 9 }, { 16, 10 }, { 21, 9 }, { 32, 9 } };
    int i, a, n = 0;
    ShPlan p;
    char what[64];
    for (i = 0; i < (int)(sizeof kH / sizeof kH[0]); ++i)
        for (a = 0; a < (int)(sizeof kAsp / sizeof kAsp[0]); ++a) {
            const int h = kH[i], w = h * kAsp[a][0] / kAsp[a][1] + (i & 1);  // odd widths too
            sprintf(what, "%dx%d", w, h);
            CHECK(ShMakePlan(SH_SOFT, w, h, SH_SOFT_PX720, &p) == 1, "plan %s", what);
            CheckPlanShape(&p, what);
            CHECK(!p.clamped && fabsf(p.width - p.target) <= SH_WIDTH_TOL * p.target, "%s: width %.2f for %.2f", what, p.width, p.target);
            if (a == 1) printf("  plan %s: down to %dx%d (%d level(s)), %d blur pass(es) (last offset %.3f), %d draws: %.2f px (asked %.2f), %.1f MB\n",
                               what, p.w[p.levels], p.h[p.levels], p.levels, p.kawase, p.kawase ? p.off[p.kawase - 1] : 0.0f, p.nsteps,
                               p.width, p.target, ShChainBytes(&p) / 1048576.0);
            ++n;
        }
    CHECK(n == 55, "cases %d", n);
    // the user's 2560x1440: 24 px, down to the quarter size or smaller, about 7-8 draws, a few MB
    ShMakePlan(SH_SOFT, 2560, 1440, SH_SOFT_PX720, &p);
    CHECK(p.levels >= 2 && p.nsteps <= 9 && fabsf(p.width - 24.0f) < 0.72f && ShChainBytes(&p) < 6u * 1048576u,
          "1440: %d levels, %d draws, %.2f px, %.1f MB", p.levels, p.nsteps, p.width, ShChainBytes(&p) / 1048576.0);
    CHECK((ShPlanBufs(&p) & (1u << SHB_X)) == (p.kawase > 0 ? (1u << SHB_X) : 0u), "the ping-pong target only with blur passes");
    // hidden SoftPx: every value of its range gives a plan of the right shape; within 3 % from 6 px at 720 lines up
    {
        float px;
        for (px = SH_PX720_MIN; px <= SH_PX720_MAX + 1e-4f; px += 0.5f) {
            sprintf(what, "SoftPx %.1f at 1440", px);
            CHECK(ShMakePlan(SH_SOFT, 2560, 1440, px, &p) == 1, "%s", what);
            CheckPlanShape(&p, what);
            if (px >= 3.0f) CHECK(!p.clamped, "%s: %.2f for %.2f", what, p.width, p.target);
            else CHECK(p.width >= p.target, "%s: the least the chain does (%.2f for %.2f)", what, p.width, p.target);
        }
        ShMakePlan(SH_SOFT, 640, 480, SH_PX720_MIN, &p);
        CHECK(p.clamped && p.levels == 1 && p.kawase == 0, "SoftPx 2 at 480: the chain's least (%.2f for %.2f), logged as the nearest", p.width, p.target);
    }
    CHECK(ShMakePlan(SH_STOCK, 2560, 1440, SH_SOFT_PX720, &p) == 0 && p.levels == 0, "Stock: no plan");
    CHECK(ShMakePlan(SH_SOFT, 10, 10, SH_SOFT_PX720, &p) == 0, "tiny");
    CHECK(ShMakePlan(SH_SOFT, 64, 40, SH_SOFT_PX720, &p) == 1 && p.levels <= 2, "small: fewer levels (%d)", p.levels);
    CheckPlanShape(&p, "64x40");
    CHECK(ShCopyBytes(2560, 1440) == 2u * 2560u * 1440u * 4u && ShCopyBytes(0, 5) == 0 && ShPicBytes(2560, 1440) == 2560u * 1440u * 4u, "copies");
}

// ---------------------------------------------------------------- 2D: the exact draws on a patch, against v3.40's Softer
#define P2 320
static float g_buf2[SHB_N][P2 * P2];
static float Tap2(const float *t, int w, int h, float u, float v)
{
    const float sx = u * (float)w - 0.5f, sy = v * (float)h - 0.5f, fx0 = floorf(sx), fy0 = floorf(sy), fx = sx - fx0, fy = sy - fy0;
    int x0 = (int)fx0, y0 = (int)fy0, x1 = x0 + 1, y1 = y0 + 1;
    x0 = x0 < 0 ? 0 : x0 > w - 1 ? w - 1 : x0, x1 = x1 < 0 ? 0 : x1 > w - 1 ? w - 1 : x1;
    y0 = y0 < 0 ? 0 : y0 > h - 1 ? h - 1 : y0, y1 = y1 < 0 ? 0 : y1 > h - 1 ? h - 1 : y1;
    return (t[y0 * w + x0] * (1 - fx) + t[y0 * w + x1] * fx) * (1 - fy) + (t[y1 * w + x0] * (1 - fx) + t[y1 * w + x1] * fx) * fy;
}
static void Draw2(const float *src, int sw, int sh, float *dst, int dw, int dh, float du, float dv, int comp)
{
    int x, y;
    for (y = 0; y < dh; ++y)
        for (x = 0; x < dw; ++x) {
            const float u = comp ? ((float)x - SH_COMP_X0) / (float)dw : ((float)x + 0.5f) / (float)dw;
            const float v = comp ? ((float)y - SH_COMP_X0) / (float)dh : ((float)y + 0.5f) / (float)dh;
            dst[y * dw + x] = 0.25f * (Tap2(src, sw, sh, u - du, v - dv) + Tap2(src, sw, sh, u + du, v - dv) +
                                       Tap2(src, sw, sh, u - du, v + dv) + Tap2(src, sw, sh, u + du, v + dv));
        }
}
static void Run2D(const ShPlan *tmpl, const float *mask, float *out)  // the plan's draws on a P2 x P2 patch; out = core(T)
{
    ShPlan p;
    int k, i;
    memset(&p, 0, sizeof p);
    p.levels = tmpl->levels, p.kawase = tmpl->kawase;
    memcpy(p.off, tmpl->off, sizeof p.off);
    p.w[0] = p.h[0] = P2;
    for (k = 1; k <= p.levels; ++k) p.w[k] = p.h[k] = ShHalf(p.w[k - 1]);
    ShBuildSteps(&p);
    memcpy(g_buf2[SHB_MASK], mask, sizeof(float) * P2 * P2);
    for (k = 0; k < p.nsteps; ++k) {
        const ShStep *s = &p.step[k];
        Draw2(g_buf2[s->src], s->sw, s->sh, s->kind == SHS_COMP ? out : g_buf2[s->dst], s->dw, s->dh, s->du, s->dv, s->kind == SHS_COMP);
    }
    for (i = 0; i < P2 * P2; ++i) out[i] = ShCore(out[i]);
}
static void RunOld340(const float *mask, float *out)  // v3.40 Softer at 1440: 2 box passes, one 4x composite, taps 0.82
{
    static float b1[P2 * P2], b2[P2 * P2];
    const int w1 = ShHalf(P2), w2 = ShHalf(w1);
    int i;
    Draw2(mask, P2, P2, b1, w1, w1, 1.0f / P2, 1.0f / P2, 0);
    Draw2(b1, w1, w1, b2, w2, w2, 1.0f / (float)w1, 1.0f / (float)w1, 0);
    Draw2(b2, w2, w2, out, P2, P2, 0.82f / (float)w2, 0.82f / (float)w2, 1);
    for (i = 0; i < P2 * P2; ++i) out[i] = out[i] + out[i] * (1.0f - out[i]);
}
static void Gauss2D(const float *in, float sigma, float *out)  // separable, CLAMP, then the core curve
{
    static float tmp[P2 * P2], kern[256];
    const int r = (int)ceilf(4.0f * sigma) < 127 ? (int)ceilf(4.0f * sigma) : 127;
    float s = 0.0f;
    int i, x, y;
    for (i = -r; i <= r; ++i) s += kern[i + r] = expf(-0.5f * (float)(i * i) / (sigma * sigma));
    for (i = 0; i <= 2 * r; ++i) kern[i] /= s;
    for (y = 0; y < P2; ++y)
        for (x = 0; x < P2; ++x) {
            float v = 0.0f;
            for (i = -r; i <= r; ++i) v += kern[i + r] * in[y * P2 + (x + i < 0 ? 0 : x + i > P2 - 1 ? P2 - 1 : x + i)];
            tmp[y * P2 + x] = v;
        }
    for (y = 0; y < P2; ++y)
        for (x = 0; x < P2; ++x) {
            float v = 0.0f;
            for (i = -r; i <= r; ++i) v += kern[i + r] * tmp[(y + i < 0 ? 0 : y + i > P2 - 1 ? P2 - 1 : y + i) * P2 + x];
            out[y * P2 + x] = ShCore(v);
        }
}
// "blocky": along a slanted straight edge every pixel at the same distance from the edge should be equally dark; the
// grid of a small picture stretched up shows as differences between them. rms / max of (darkening - the mean of its
// 0.05 px distance bin), the middle of the patch, as a share of the inside darkness
#define NBIN 8000
static void Blocky(const float *d, float ang, float *rms, float *mx)
{
    static double sum[NBIN];
    static int cnt[NBIN];
    const float nx = cosf(ang), ny = sinf(ang), c = P2 * 0.5f + 0.37f;
    double s2 = 0.0;
    float m = 0.0f, top = 0.0f;
    int x, y, n = 0;
    memset(sum, 0, sizeof sum), memset(cnt, 0, sizeof cnt);
    for (y = 0; y < P2 * P2; ++y) if (d[y] > top) top = d[y];
    for (y = P2 / 4; y < 3 * P2 / 4; ++y)
        for (x = P2 / 4; x < 3 * P2 / 4; ++x) {
            const int b = (int)floorf((((float)x - c) * nx + ((float)y - c) * ny) / 0.05f) + NBIN / 2;
            if (b >= 0 && b < NBIN) sum[b] += d[y * P2 + x] / top, ++cnt[b];
        }
    for (y = P2 / 4; y < 3 * P2 / 4; ++y)
        for (x = P2 / 4; x < 3 * P2 / 4; ++x) {
            const int b = (int)floorf((((float)x - c) * nx + ((float)y - c) * ny) / 0.05f) + NBIN / 2;
            if (b >= 0 && b < NBIN && cnt[b] > 1) {
                const float r = (float)fabs(d[y * P2 + x] / top - sum[b] / cnt[b]);
                s2 += (double)r * r, ++n;
                if (r > m) m = r;
            }
        }
    *rms = n ? (float)sqrt(s2 / n) : 0.0f, *mx = m;
}
static void Test2D(void)
{
    static float mask[P2 * P2], dn[P2 * P2], dold[P2 * P2];
    static const float kAng[] = { 0.2618f, 0.5236f, 0.7854f, 1.1345f };  // 15, 30, 45, 65 degrees
    ShPlan p;
    int k, x, y;
    ShMakePlan(SH_SOFT, 2560, 1440, SH_SOFT_PX720, &p);  // the user's resolution
    for (k = 0; k < (int)(sizeof kAng / sizeof kAng[0]); ++k) {
        const float nx = cosf(kAng[k]), ny = sinf(kAng[k]), c = P2 * 0.5f + 0.37f;
        float rn, mn, ro, mo;
        for (y = 0; y < P2; ++y)
            for (x = 0; x < P2; ++x) mask[y * P2 + x] = ((float)x - c) * nx + ((float)y - c) * ny >= 0.0f ? 1.0f : 0.0f;  // hard, as the stencil
        Run2D(&p, mask, dn);
        RunOld340(mask, dold);
        Blocky(dn, kAng[k], &rn, &mn);
        Blocky(dold, kAng[k], &ro, &mo);
        printf("  blocky at %2.0f deg (share of the inside darkness): v3.40 Softer rms %.4f, max %.4f; new Soft rms %.4f, max %.4f\n",
               kAng[k] * 57.29578f, ro, mo, rn, mn);
        CHECK(rn < 0.25f * ro && mn < 0.25f * mo, "the grid along a %.0f deg edge: new %.4f / %.4f, v3.40 %.4f / %.4f", kAng[k] * 57.29578f, rn, mn, ro, mo);
        CHECK(mn < 0.006f, "the new edge's largest grid step %.4f (1/255 of a 35 %% shadow is 0.011)", mn);
    }
    {   // the other resolutions' plans (other level / pass mixes): the same bound
        static const int kH[] = { 480, 720, 1080, 1200, 2160 };
        for (k = 0; k < (int)(sizeof kH / sizeof kH[0]); ++k) {
            const float ang = 0.2618f, nx = cosf(ang), ny = sinf(ang), c = P2 * 0.5f + 0.37f;
            float rn, mn, ro, mo;
            ShMakePlan(SH_SOFT, kH[k] * 16 / 9, kH[k], SH_SOFT_PX720, &p);
            for (y = 0; y < P2; ++y)
                for (x = 0; x < P2; ++x) mask[y * P2 + x] = ((float)x - c) * nx + ((float)y - c) * ny >= 0.0f ? 1.0f : 0.0f;
            Run2D(&p, mask, dn);
            Blocky(dn, ang, &rn, &mn);
            Gauss2D(mask, p.width / 2.11f, dold);  // the floor: an exact Gaussian of the same width on the same hard mask
            Blocky(dold, ang, &ro, &mo);
            printf("  blocky at 15 deg, %d lines (%d level(s), %d pass(es)): rms %.4f, max %.4f (an exact Gaussian: %.4f, %.4f)\n",
                   kH[k], p.levels, p.kawase, rn, mn, ro, mo);
            CHECK(mn < 1.5f * mo + 0.002f && rn < 2.5f * ro + 0.0003f, "%d lines: the grid along a slanted edge %.4f / %.4f (Gaussian %.4f / %.4f)",
                  kH[k], rn, mn, ro, mo);
        }
    }
}

// ---------------------------------------------------------------- darkness
static void TestDarkness(void)
{
    static const unsigned kClear[] = { 0x00000000u, 0x80303030u, 0x00402010u, 0xFF8080FFu, 0x7F102030u };
    static const float kPe[] = { 1.0f, 1.5f, 0.5f, 3.0f, 0.25f };
    int st, cut, c, k, dv, cases = 0;
    for (st = 0; st <= 12; ++st)
        for (cut = 0; cut <= 3; ++cut)
            for (c = 0; c < (int)(sizeof kClear / sizeof kClear[0]); ++c)
                for (k = 0; k < (int)(sizeof kPe / sizeof kPe[0]); ++k) {
                    static const int kCut[4] = { 0, 2, 9, 0x43 };
                    const unsigned tf = ShPeScale(ShStockTf(st, st == 4 && c == 1 ? 0x1F : 2, kClear[c], kCut[cut]), kPe[k]);
                    const float a = (float)ShTfAlpha(tf) / 255.0f;  // the mask's 8-bit alpha = TEXTUREFACTOR's
                    float col[4], cc, t, tin;
                    if (!ShTfAlpha(tf)) continue;                   // no darkness: the hook draws stock (nothing to change)
                    ShTfColour(tf, col);
                    cc = ShNormC(a);
                    // the first pass as the GPU does it: mul_x4 by c, then add_sat x 3; written to an 8-bit target
                    t = 4.0f * cc * a;
                    t = fminf(1.0f, 2.0f * t), t = fminf(1.0f, 2.0f * t), t = fminf(1.0f, 2.0f * t);
                    tin = floorf(t * 255.0f + 0.5f) / 255.0f;
                    CHECK(cc > 0.0f && cc <= 1.0f && ShCompK(a) > 0.0f && ShCompK(a) <= 1.0f, "constants in range %08X: c %.4f K %.4f", tf, cc, ShCompK(a));
                    CHECK(fabsf(ShNormT(a, cc) - t) < 1e-6f, "ShNormT = the shader's steps %08X", tf);
                    if (a >= 8.0f / 255.0f) CHECK(tin == 1.0f, "inside coverage exactly 1 at %08X (%.6f)", tf, tin);
                    CHECK(fabsf(ShSoftAlphaT(a, tin) - ShStockAlpha(a)) < 1e-6f, "inside alpha %08X: %.6f / %.6f", tf, ShSoftAlphaT(a, tin), ShStockAlpha(a));
                    for (dv = 0; dv <= 8; ++dv) {
                        const float dst = (float)dv / 8.0f;
                        int ch;
                        for (ch = 0; ch < 3; ++ch) {
                            const float s = ShStockPixel(dst, col[ch], a, 1.0f, 1.0f), o = ShSoftPixelT(dst, col[ch], a, tin);
                            CHECK(fabsf(s - o) < 1e-5f, "inside %08X dst %.3f ch %d: stock %.5f soft %.5f", tf, dst, ch, s, o);
                        }
                    }
                    ++cases;
                }
    CHECK(cases > 900, "cases %d", cases);
    // the table's values (KB 52 / research 1): stage 1 0x32, stage 4 room 0x1F none, the tinted stages keep the colour
    CHECK(ShStockTf(1, 2, 0, 0) == 0x32000000u && ShStockTf(4, 0x1F, 0, 0) == 0 && ShStockTf(4, 2, 0, 0) == 0x50000000u, "table");
    CHECK(ShStockTf(10, 1, 0x00402010u, 0) == ShHalfColour(0x00402010u, 0x5A) && (ShStockTf(10, 1, 0x00402010u, 0) & 0xFFFFFF) == 0x201008u,
          "stage 10 tint %08X", ShStockTf(10, 1, 0x00402010u, 0));
    CHECK(ShStockTf(9, 1, 0, 0) == ShStockTf(10, 1, 0, 0), "9 falls into 10");
    CHECK(ShTfAlpha(ShStockTf(11, 1, 0, 0)) == 0x14, "stage 11 alpha 0x28 >> 1: %02X", ShTfAlpha(ShStockTf(11, 1, 0, 0)));
    CHECK(ShStockTf(1, 2, 0, 9) == 0x78000000u && ShStockTf(1, 2, 0, 0x43) == 0 && ShStockTf(3, 2, 0, 0x32) == 0x14000000u, "cut-scenes");
    CHECK(ShTfAlpha(ShPeScale(0x32000000u, 1.5f)) == 75 && ShTfAlpha(ShPeScale(0xC0000000u, 1.5f)) == 255 &&
          (ShPeScale(0x2D201008u, 1.5f) & 0xFFFFFF) == 0x201008u, "Playable Eileen's 150 %%");
    {   // the shader's colour constant: (r, g, b, 0)
        float c2[4];
        ShTfColour(0x5A804020u, c2);
        CHECK(fabsf(c2[0] - 128.0f / 255.0f) < 1e-6f && fabsf(c2[1] - 64.0f / 255.0f) < 1e-6f && fabsf(c2[2] - 32.0f / 255.0f) < 1e-6f &&
              c2[3] == 0.0f, "c2");
    }
    {   // outside (T = 0) untouched; across the edge the darkening grows smoothly up to stock's, never darker
        float t, last = -1.0f;
        const float a = 75.0f / 255.0f;
        int ok = 1;
        CHECK(ShSoftPixelT(0.7f, 0.0f, a, 0.0f) == 0.7f, "outside");
        for (t = 0.0f; t <= 1.0f + 1e-6f; t += 0.01f) {
            const float A = ShSoftAlphaT(a, t);
            ok &= A >= last && A <= ShStockAlpha(a) + 1e-6f;
            last = A;
        }
        CHECK(ok, "edge alpha monotone, never darker than inside");
        CHECK(ShCore(0.0f) == 0.0f && ShCore(1.0f) == 1.0f && fabsf(ShCore(0.25f) - 0.4375f) < 1e-6f, "core curve");
    }
}

// thin shapes at 1440 lines: a bar of b pixels (1D: a long leg / an arm / a pipe) through the chain; the darkest point as
// a share of the inside darkness, with the core curve and (for comparison) a straight ramp; and v3.40's Softer
static float BarPeak(const ShPlan *p, int b, int core)
{
    static float m[SH_STRIP], o[SH_STRIP];
    float best = 0.0f;
    int i;
    for (i = 0; i < SH_STRIP; ++i) m[i] = i >= SH_STRIP / 2 - b / 2 && i < SH_STRIP / 2 - b / 2 + b ? 1.0f : 0.0f;
    ShRun1D(p->levels, p->kawase, p->off, m, SH_STRIP, o);
    for (i = 0; i < SH_STRIP; ++i) {
        const float v = core ? o[i] : 1.0f - sqrtf(1.0f - o[i]);  // core^-1 = the straight ramp of the same blur
        if (v > best) best = v;
    }
    return best;
}
static float BarPeakOld(int b)  // v3.40 Softer at 1440 (2 box passes, composite taps 0.82 quarter texels), x + x (1 - x) on a x
{
    static float m[SH_STRIP], q[SH_STRIP], r[SH_STRIP];
    const float a = 75.0f / 255.0f;
    float best = 0.0f;
    int i, n;
    for (i = 0; i < SH_STRIP; ++i) m[i] = i >= SH_STRIP / 2 - b / 2 && i < SH_STRIP / 2 - b / 2 + b ? a : 0.0f;
    n = ShDown1D(m, SH_STRIP, q);
    n = ShDown1D(q, n, r);
    for (i = 0; i < SH_STRIP; ++i) {
        const float u = ((float)i - SH_COMP_X0) / (float)SH_STRIP;
        const float x = 0.5f * (ShTap(r, n, u - 0.82f / (float)n) + ShTap(r, n, u + 0.82f / (float)n));
        const float v = (x + x * (1.0f - x)) / ShStockAlpha(a);
        if (v > best) best = v;
    }
    return best;
}
static void TestThin(void)
{
    static const int kB[] = { 6, 20, 40, 200 };
    ShPlan p;
    int k;
    ShMakePlan(SH_SOFT, 2560, 1440, SH_SOFT_PX720, &p);
    for (k = 0; k < 4; ++k) {
        const float core = BarPeak(&p, kB[k], 1), lin = BarPeak(&p, kB[k], 0), old = BarPeakOld(kB[k]);
        printf("  1440 lines, a %3d px bar: darkest %.0f %% of the inside (new Soft; a straight ramp would give %.0f %%; v3.40 Softer %.0f %%)\n",
               kB[k], 100.0f * core, 100.0f * lin, 100.0f * old);
        CHECK(core >= lin - 1e-6f, "the core curve keeps more of a %d px bar (%.3f / %.3f)", kB[k], core, lin);
        if (kB[k] >= 200) CHECK(core > 0.9999f, "a large shape's inside is stock's (%.5f)", core);
    }
    CHECK(BarPeak(&p, 20, 1) > 0.75f && BarPeak(&p, 6, 1) > 0.3f, "thin parts keep a visible shadow (20 px %.2f, 6 px %.2f)",
          BarPeak(&p, 20, 1), BarPeak(&p, 6, 1));
}

static void TestHookAndPreview(void)
{
    ShPrevIn in;
    ShKeepIn kp;
    int why = -1;
    CHECK(ShCallOk(4, 0, 4) && !ShCallOk(4, 0, 2) && !ShCallOk(5, 0, 4) && !ShCallOk(4, 6, 4), "call check");
    memset(&in, 0, sizeof in);
    CHECK(ShPreviewMode(&in, &why) == SHP_NONE && why == SHW_OFF, "off");
    in.previewOn = 1;
    CHECK(ShPreviewMode(&in, &why) == SHP_NONE && why == SHW_NOHOOK, "no hook");
    in.backdropHook = 1;
    CHECK(ShPreviewMode(&in, &why) == SHP_CARD && why == SHW_TITLE, "title -> card");
    in.pausedGame = 1;
    CHECK(ShPreviewMode(&in, &why) == SHP_CARD && why == SHW_NOCOPY, "no copy -> card");
    in.haveCopies = 1, in.copiesFrame = 10, in.pauseFrame = 12;
    CHECK(ShPreviewMode(&in, &why) == SHP_CARD && why == SHW_STALE, "stale -> card");
    in.pauseFrame = 10;
    CHECK(ShPreviewMode(&in, &why) == SHP_SCENE && why == SHW_SCENE, "scene");
    in.failed = 1;
    CHECK(ShPreviewMode(&in, &why) == SHP_CARD && why == SHW_FAILED, "failed -> card");
    in.pausedGame = 0, in.failed = 0;
    CHECK(ShPreviewMode(&in, &why) == SHP_CARD && why == SHW_TITLE, "copies but the title -> card");
    memset(&kp, 0, sizeof kp);
    CHECK(!ShCopiesFree(&kp, &why) && why == 0, "nothing to free");
    kp.haveCopies = 1, kp.gameplay = 1, kp.phase = 0, kp.capture = 1, kp.copiesFrame = kp.pauseFrame = 5;
    CHECK(!ShCopiesFree(&kp, &why), "the capture frame keeps them");
    kp.phase = 2, kp.capture = 2;
    CHECK(!ShCopiesFree(&kp, &why), "the pause keeps them (Options, the page, back, the page again)");
    kp.phase = 0, kp.capture = 0;
    CHECK(ShCopiesFree(&kp, &why) && why == 1, "play again ends them");
    kp.phase = 1;
    CHECK(ShCopiesFree(&kp, &why) && why == 1, "a cut-scene again ends them");
    kp.phase = 2, kp.capture = 2, kp.gameplay = 0;
    CHECK(ShCopiesFree(&kp, &why) && why == 2, "the title ends them");
    kp.gameplay = 1, kp.pauseFrame = 9;
    CHECK(ShCopiesFree(&kp, &why) && why == 3, "a newer pause without a copy ends them");
    CHECK(kShFail[SHF_RT][0] && kShWhy[SHW_STALE][0] && kShFreeWhy[3][0] && kShStepName[SHS_COMP][0], "texts");
    // v3.41 SH341: the pause's full-size targets stay for the next pause unless the address space runs low
    CHECK(ShKeepTargets(1300) && ShKeepTargets(SH_KEEP_MARGIN_MB) && !ShKeepTargets(SH_KEEP_MARGIN_MB - 1) && !ShKeepTargets(0), "keep rule");
}

static void TestFigure(void)
{
    static float xy[SH_FIG_TRIS * 6];
    const int n = ShFigure(640.0f, 200.0f, 300.0f, xy);
    int i, ok = 1;
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    CHECK(n > 16 && n <= SH_FIG_TRIS, "triangles %d", n);
    for (i = 0; i < n * 3; ++i) {
        const float x = xy[2 * i], y = xy[2 * i + 1];
        ok &= x == x && y == y;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    CHECK(ok && y0 >= 200.0f - 0.01f && y1 <= 500.0f + 0.01f && x0 >= 640.0f - 0.2f * 300.0f && x1 <= 640.0f + 0.2f * 300.0f,
          "bounds %.1f..%.1f x %.1f..%.1f", x0, x1, y0, y1);
    CHECK(fabsf((x0 + x1) * 0.5f - 640.0f) < 0.5f, "centred");
}

// v3.42 SM342: the Darkness row (100 / 85 / 70 / 55 %, default 85, on top of Playable Eileen's ShadowDarkness): the ini
// value, the row's nearest value, and the inside of a shadow for every stage's darkness (tinted 9-11, cut-scenes, PE 25 ..
// 300 %) at each value: Soft = exactly d x the game's inside, Stock (8-bit TFACTOR byte) within half a byte step; the
// colour of the darkening kept; 100 % = the game's draw untouched (byte 255). Prints the per-stage table (PE 100 / 150 %).
static void TestDark342(void)
{
    static const unsigned kClear[] = { 0x00000000u, 0x80303030u, 0x00402010u, 0xFF8080FFu, 0x7F102030u };
    static const float kPe[] = { 1.0f, 1.5f, 0.5f, 3.0f, 0.25f };
    static const int kPct[] = { 100, 85, 70, 55, 0, 40, 99, 1 };
    int st, cut, c, k, q, pct, cases = 0, worstAt = 0;
    float worst = 0.0f;
    {   // the ini value and the row
        CHECK(ShParseDark("85", &pct) && pct == 85, "parse 85");
        CHECK(ShParseDark("70%", &pct) && pct == 70, "parse 70%%");
        CHECK(ShParseDark(" 55 % ", &pct) && pct == 55, "parse ' 55 %% '");
        CHECK(ShParseDark("100", &pct) && pct == 100 && ShParseDark("0", &pct) && pct == 0, "parse 100 / 0");
        CHECK(ShParseDark("150", &pct) && pct == 100, "parse 150 -> 100");
        CHECK(!ShParseDark("", &pct) && pct == SH_DARK_DEF && !ShParseDark("abc", &pct) && pct == SH_DARK_DEF &&
              !ShParseDark("-5", &pct) && pct == SH_DARK_DEF && !ShParseDark("85x", &pct) && pct == SH_DARK_DEF &&
              !ShParseDark(NULL, &pct) && pct == SH_DARK_DEF, "not a number: the default 85");
        CHECK(SH_DARK_DEF == 85 && kShDarkPct[ShDarkIndex(SH_DARK_DEF)] == 85, "default 85 %% is a row value");
        for (k = 0; k < SH_DARK_N; ++k) CHECK(ShDarkIndex(kShDarkPct[k]) == k, "row value %d -> index %d", kShDarkPct[k], k);
        CHECK(ShDarkIndex(48) == 2 && ShDarkIndex(47) == 1 && ShDarkIndex(77) == 3 && ShDarkIndex(78) == 4 && ShDarkIndex(93) == 5 &&
              ShDarkIndex(92) == 4, "nearest row value (48 -> 55, 47 -> 40, 77 -> 70, 78 -> 85, 93 -> 100)");  // v3.52 SH352: the new indices
        // v3.43 SH343: the row 100 / 85 / 70 / 55 / 40 / 25, default 85; the ini's 0..100 keeps its meaning
        CHECK(SH_DARK_N == 6 && kShDarkPct[0] == 25 && kShDarkPct[1] == 40 && kShDarkPct[2] == 55 && kShDarkPct[3] == 70 &&
              kShDarkPct[4] == 85 && kShDarkPct[5] == 100, "v3.52: the row's six values, 25 .. 100 (was 100 .. 25)");
        CHECK(ShDarkIndex(40) == 1 && ShDarkIndex(25) == 0 && ShDarkIndex(33) == 1 && ShDarkIndex(32) == 0 && ShDarkIndex(0) == 0 &&
              ShDarkIndex(10) == 0, "v3.43: nearest (40, 25, 33 -> 40, 32 -> 25, 0 / 10 -> 25)");  // v3.52 SH352: the new indices
        CHECK(ShParseDark("40", &pct) && pct == 40 && ShParseDark("25 %", &pct) && pct == 25 && ShParseDark("10", &pct) && pct == 10,
              "v3.43: parse 40 / 25 / 10 (any 0..100 used as it is)");
        for (k = 1; k < SH_DARK_N; ++k) CHECK(kShDarkPct[k] > kShDarkPct[k - 1], "v3.52: the row darkens step by step to the right (%d)", k);
        CHECK(ShDarkD(100) == 1.0f && ShDarkD(0) == 0.0f && ShDarkD(150) == 1.0f && ShDarkD(-3) == 0.0f && fabsf(ShDarkD(85) - 0.85f) < 1e-6f, "d");
    }
    for (st = 0; st <= 12; ++st)
        for (cut = 0; cut <= 3; ++cut)
            for (c = 0; c < (int)(sizeof kClear / sizeof kClear[0]); ++c)
                for (k = 0; k < (int)(sizeof kPe / sizeof kPe[0]); ++k) {
                    static const int kCut[4] = { 0, 2, 9, 0x43 };
                    const unsigned tf = ShPeScale(ShStockTf(st, st == 4 && c == 1 ? 0x1F : 2, kClear[c], kCut[cut]), kPe[k]);
                    const int a8 = ShTfAlpha(tf);
                    const float a = (float)a8 / 255.0f, full = ShStockAlpha(a);
                    float col[4], prevSoft = 2.0f, prevStock = 2.0f, t, tin;
                    if (!a8) continue;  // no darkness: the hook draws stock whatever the row says
                    ShTfColour(tf, col);
                    t = 4.0f * ShNormC(a) * a;  // the inside coverage as the shader makes it (TestDarkness: exactly 1 from a8 = 8)
                    t = fminf(1.0f, 2.0f * t), t = fminf(1.0f, 2.0f * t), t = fminf(1.0f, 2.0f * t);
                    tin = floorf(t * 255.0f + 0.5f) / 255.0f;
                    CHECK(ShStockDarkByte(a8, 1.0f) == 255u && ShStockDarkByte(a8, ShDarkD(100)) == 255u, "100 %%: the game's draw untouched %08X", tf);
                    CHECK(ShStockDarkByte(a8, 0.0f) == 0u, "0 %%: no shadow %08X", tf);
                    for (q = 0; q < (int)(sizeof kPct / sizeof kPct[0]); ++q) {
                        const float d = ShDarkD(kPct[q]), want = d * full;
                        const float soft = ShCompKDark(a, d) * ShCore(tin);
                        const unsigned s8 = ShStockDarkByte(a8, d);
                        const float stock = ShStockDarkInside(a8, s8), err = fabsf(stock - want);
                        int dv, ch;
                        CHECK(fabsf(soft - d * ShSoftAlphaT(a, tin)) < 1e-6f, "soft inside %08X at %d %%: d x its 100 %%", tf, kPct[q]);
                        if (a8 >= 8) CHECK(fabsf(soft - want) < 1e-6f, "soft inside %08X at %d %%: %.6f (want %.6f)", tf, kPct[q], soft, want);
                        // half a byte step of the copies' factor moves the inside by at most a / 255
                        CHECK(err <= a / 255.0f + 1e-6f, "stock inside %08X at %d %%: %.5f (want %.5f, byte %u)", tf, kPct[q], stock, want, s8);
                        if (want > 0.02f && err / want > worst) worst = err / want, worstAt = (int)tf;
                        for (dv = 0; dv <= 4; ++dv) {  // the darkening's colour: the same share in every channel (a tint stays a tint)
                            const float dst = 0.25f * (float)dv;
                            for (ch = 0; ch < 3; ++ch) {
                                const float o = ShSoftPixelT(dst, col[ch], a, tin), od = dst * (1.0f - soft) + col[ch] * soft;
                                CHECK(fabsf((od - dst) - d * (o - dst)) < 1e-5f, "soft darkening x d %08X ch %d dst %.2f", tf, ch, dst);
                            }
                        }
                        if (kPct[q] <= 100 && q < 4) {  // the row's values in order: each lighter than the one before
                            CHECK(soft < prevSoft + 1e-7f && stock <= prevStock + 1e-6f, "order %08X at %d %%", tf, kPct[q]);
                            prevSoft = soft, prevStock = stock;
                        }
                        ++cases;
                    }
                }
    CHECK(cases > 7000, "cases %d", cases);
    CHECK(worst < 0.012f, "stock: the worst relative error of the inside %.2f %% (%08X)", 100.0f * worst, (unsigned)worstAt);
    printf("darkness (v3.42): %d cases; Stock's worst relative error of the inside %.2f %% (darkness %08X)\n", cases, 100.0f * worst,
           (unsigned)worstAt);
    {   // the table: the inside alpha per stage (room 2, no cut-scene, clear colour 0x402010 for the tinted stages), PE 100 / 150 %
        static const float kPe2[2] = { 1.0f, 1.5f };
        printf("darkness table: the inside (share of the picture darkened: 1 - (1 - a)^2 x d); Stock (TFACTOR byte) / Soft\n");
        for (st = 1; st <= 11; ++st)
            for (k = 0; k < 2; ++k) {
                const unsigned tf = ShPeScale(ShStockTf(st, 2, 0x00402010u, 0), kPe2[k]);
                const int a8 = ShTfAlpha(tf);
                char line[256];
                int o = 0;
                o += sprintf(line + o, "  stage %2d PE %3.0f %%: %08X game %.3f |", st, 100.0f * kPe2[k], tf, ShStockAlpha((float)a8 / 255.0f));
                for (q = 0; q < SH_DARK_N; ++q) {
                    const float d = ShDarkD(kShDarkPct[q]);
                    const unsigned s8 = ShStockDarkByte(a8, d);
                    o += sprintf(line + o, " %3d %%: %.3f (x%3u) / %.3f |", kShDarkPct[q], ShStockDarkInside(a8, s8), s8,
                                 ShCompKDark((float)a8 / 255.0f, d));
                }
                printf("%s\n", line);
            }
    }
}

// v3.43 SH343 (USER_FEEDBACK_v342 item 4: Darkness 55 % lightens the page's right half, play "no visible change"): the
// in-play composite and the page's right half take c1.a from ShCompC1 with the same inputs (the frame's TEXTUREFACTOR
// alpha read at 0x55FE43, which the pause copy keeps as g_shCapTf; the Darkness percent): the same bits for every alpha
// and percent, equal to v3.42's K x d; the inside is d x the 100 % inside; the read-back check and its schedule
static void TestC1343(void)
{
    int a8, pct, n, cases = 0;
    unsigned key, last;
    for (a8 = 0; a8 <= 255; ++a8)
        for (pct = 0; pct <= 100; ++pct) {
            const unsigned tfLive = (unsigned)a8 << 24 | 0x00402010u, capTf = tfLive;  // ShCapture keeps the hook's own tf
            const float play = ShCompC1(ShTfAlpha(tfLive), pct), page = ShCompC1(ShTfAlpha(capTf), pct);
            const float v342 = ShCompKDark((float)a8 / 255.0f, ShDarkD(pct));
            CHECK(memcmp(&play, &page, sizeof play) == 0, "play %.6f != page %.6f (a8 %d, %d %%)", play, page, a8, pct);
            CHECK(memcmp(&play, &v342, sizeof play) == 0, "c1 %.6f != v3.42's K x d %.6f (a8 %d, %d %%)", play, v342, a8, pct);
            if (a8 >= 8) {
                const float full = ShCompC1(a8, 100), inside = play * ShCore(1.0f);
                CHECK(fabsf(inside - ShDarkD(pct) * full) < 1e-6f, "inside at %d %% = d x the 100 %% inside (a8 %d)", pct, a8);
                CHECK(fabsf(full - ShStockAlpha((float)a8 / 255.0f)) < 1e-6f, "100 %% = the game's inside (a8 %d)", a8);
            }
            CHECK(ShC1Same(play, play) && ShC1Same(play, floorf(play * 255.0f + 0.5f) / 255.0f), "read back exact / 8-bit");
            CHECK(!ShC1Same(play, play + 1.0f / 255.0f) && !ShC1Same(play, -1.0f), "a read back off by a byte step: not the same");
            ++cases;
        }
    for (a8 = 1; a8 <= 255; ++a8) {  // the six row values: each darker than the one before (v3.52 SH352: was lighter), Soft and Stock
        int k;
        for (k = 1; k < SH_DARK_N; ++k) {
            const float dk = ShDarkD(kShDarkPct[k]), dp = ShDarkD(kShDarkPct[k - 1]);
            CHECK(ShCompC1(a8, kShDarkPct[k - 1]) < ShCompC1(a8, kShDarkPct[k]) + 1e-7f, "soft row order a8 %d at %d %%", a8, kShDarkPct[k]);
            CHECK(ShStockDarkByte(a8, dp) <= ShStockDarkByte(a8, dk), "stock byte order a8 %d at %d %%", a8, kShDarkPct[k]);
            CHECK(fabsf(ShStockDarkInside(a8, ShStockDarkByte(a8, dk)) - dk * ShStockAlpha((float)a8 / 255.0f)) <= (float)a8 / 255.0f / 255.0f + 1e-6f,
                  "stock inside at %d %% (a8 %d)", kShDarkPct[k], a8);
        }
    }
    CHECK(ShCompC1(300, 55) == ShCompC1(255, 55) && ShCompC1(-4, 55) == 0.0f, "alpha clamped");
    // the schedule: a new (alpha, percent) is read back at once, the same one every SH_C1_EVERY-th composite
    last = ShC1Key(0x4B, 85);
    CHECK(ShC1Due(ShC1Key(0x4B, 55), last, 7) && ShC1Due(ShC1Key(0x4C, 85), last, 7), "a new key: due");
    for (n = 1, key = 0; n <= 3 * SH_C1_EVERY; ++n) key += (unsigned)ShC1Due(last, last, (unsigned)n);
    CHECK(key == 3, "the same key: due 3 times in %d composites (%u)", 3 * SH_C1_EVERY, key);
    CHECK(ShC1Key(0x4B, 55) != ShC1Key(0x4B, 85) && ShC1Key(0x4B, 55) != ShC1Key(0x44, 55), "keys differ");
    // the user's case (PE 150 %: 4B000000): the share of the floor's light inside a shadow
    {
        const float g = ShLightThrough(ShCompC1(0x4B, 100)), l85 = ShLightThrough(ShCompC1(0x4B, 85)),
                    l55 = ShLightThrough(ShCompC1(0x4B, 55)), l40 = ShLightThrough(ShCompC1(0x4B, 40)),
                    l25 = ShLightThrough(ShCompC1(0x4B, 25));
        CHECK(fabsf(l40 - 0.799f) < 0.002f && fabsf(l25 - 0.874f) < 0.002f, "4B000000 at 40 / 25 %%: %.3f %.3f", l40, l25);
        printf("c1 (v3.43): 4B000000 at 40 %% lets through %.1f %%, at 25 %% %.1f %%\n", 100.0f * l40, 100.0f * l25);
        CHECK(fabsf(g - 0.498f) < 0.002f && fabsf(l85 - 0.574f) < 0.002f && fabsf(l55 - 0.724f) < 0.002f, "4B000000: %.3f %.3f %.3f", g, l85, l55);
        printf("c1 (v3.43): %d cases, play == page == v3.42; 4B000000 lets through %.1f %% (100 %%), %.1f %% (85 %%), %.1f %% (55 %%): "
               "85 -> 55 %% x%.2f, 100 -> 55 %% x%.2f\n", cases, 100.0f * g, 100.0f * l85, 100.0f * l55, l55 / l85, l55 / g);
    }
}

// v3.44 CS344: the composite rebuilt without the `1-x` modifier (GOG's translator dropped v3.41's), and the pixel check
static const PsTok kOldComp341[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1), I_LRP, D_R(1), S_C(0), S_T(2), S_T(3), I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),
    I_MAD, D_R(1), S_R(0), S_R(0) | S_INV, S_R(0), I_MUL, D_R(1), S_R(1), S_C(1), I_ADD, D_R(0), S_R(1), S_C(2), PS_END };
static void TestComp344(void)
{
    const char *why;
    int at, a8, pct, i, n = 0;
    float worst = 0.0f;
    CHECK(PsLint(kShCompPs, 64, &at, &why) == PSL_OK, "the new composite uses proven forms only (%s at %d)", why, at);
    CHECK(PsLint(kOldComp341, 64, &at, &why) == PSL_MOD && at == 7, "v3.41's composite: a source modifier at instruction 7 (%s %d)", why, at);
    for (a8 = 0; a8 <= 255; ++a8)  // the shader's alpha = the sim's (ShSoftAlphaT x d) for every coverage and Darkness
        for (pct = 0; pct <= 100; pct += 5)
            for (i = 0; i <= 64; ++i) {
                const float t = (float)i / 64.0f, k = ShCompC1(a8, pct), sim = ShCompKDark((float)a8 / 255.0f, ShDarkD(pct)) * ShCore(t);
                const float d = fabsf(ShCompShaderA(t, k) - sim);
                if (d > worst) worst = d;
                ++n;
            }
    CHECK(worst < 1e-5f, "the shader's alpha = the sim's (%d cases, worst %.2g)", n, worst);
    for (pct = 25; pct <= 100; pct += 15) {  // the verdicts, from what the read back would hold
        const float k = ShCompC1(0x4B, pct);
        unsigned char g[SH_PX_N];
        float a[SH_PX_N], off;
        for (i = 0; i < SH_PX_N; ++i) g[i] = (unsigned char)(255.0f * (1.0f - ShCompShaderA((float)kShPxA[i] / 255.0f, k)) + 0.5f);
        CHECK(ShPxVerdict(k, g, a, &off) == SHX_OK && fabsf(a[0] - k) <= SH_PX_TOL, "the shader's pixels: ok (%d %%: %.3f %.3f)", pct, a[0], k);
        g[1] = (unsigned char)(g[1] + 2);
        CHECK(ShPxVerdict(k, g, a, &off) == SHX_OK, "2 levels of rounding: ok (%d %%)", pct);
        for (i = 0; i < SH_PX_N; ++i) g[i] = (unsigned char)(255 - kShPxA[i]);  // the fixed-function result: alpha = the coverage
        CHECK(ShPxVerdict(k, g, a, &off) == SHX_COVER && a[0] == 1.0f, "the coverage itself: COVER, the v3.43 failure (%d %%)", pct);
        for (i = 0; i < SH_PX_N; ++i) g[i] = 255;
        CHECK(ShPxVerdict(k, g, a, &off) == SHX_NOTHING, "white kept: NOTHING (%d %%)", pct);
        for (i = 0; i < SH_PX_N; ++i) g[i] = (unsigned char)(255.0f * (1.0f - 0.5f * ShCompShaderA((float)kShPxA[i] / 255.0f, k)) + 0.5f);
        CHECK(ShPxVerdict(k, g, a, &off) == SHX_OFF, "half the asked alpha: OFF (%d %%)", pct);
    }
    {
        const float k25 = ShCompC1(0x4B, 25), k100 = ShCompC1(0x4B, 100);
        CHECK(k100 - k25 > 0.3f, "the Darkness spans a visible range on 4B000000 (%.3f .. %.3f)", k25, k100);
        printf("composite (v3.44): %d cases = the sim; 4B000000 full-cover alpha %.3f at 100 %% .. %.3f at 25 %%\n", n, k100, k25);
    }
}

// v3.47 PX347: the composite's pixel check. (1) The v3.46 log (2026-11-24, in play, Darkness 70 %, darkness 4B000000 and
// 32000000): the v3.44 pattern under a sampler half a texel left (black past the edge) with the third source cell's
// Clear lost (coverage 0) reproduces the four alphas of both lines within 0.003 (without the lost Clear it does not:
// cells 2 / 3 would read 0.302 / 0.154): the shader is its sim. (2) The v3.47 pattern passes under any offset up to
// 1 texel and measures it. (3) A lost Clear: found in the source, the shader judged on what the source held.
typedef struct { float k; } PxmShCtx;
static void PxmComp(const float in[4], float out[4], const void *ctx)
{
    out[0] = out[1] = out[2] = 0.0f;  // c2 black
    out[3] = ShCompShaderA(in[3], ((const PxmShCtx *)ctx)->k);  // four taps at one point: T = the coverage
}
static void TestPx347(void)
{
    static const float kLog[2][SH_PX_N] = { { 0.263f, 0.345f, 0.216f, 0.000f }, { 0.184f, 0.243f, 0.153f, 0.000f } };
    static const int kTf[2] = { 0x4B, 0x32 };
    static PxmTex t;
    unsigned long cells[PX_CELLS], old[PX_CELLS];
    unsigned char g[SH_PX_N];
    float a[SH_PX_N], off, tr[SH_PX_N], worst = 0.0f, keep = 0.0f, wo = 0.0f;
    PxProbe p;
    PxmShCtx cx;
    int j, k, bx, oi, oj, n, cases = 0;
    for (k = 0; k < PX_CELLS; ++k) cells[k] = (unsigned long)kShPxA[k] << 24;
    for (j = 0; j < 2; ++j) {  // (1)
        cx.k = ShCompC1(kTf[j], 70);
        CHECK(fabsf(cx.k - (j ? 0.248f : 0.351f)) < 0.0006f, "K at Darkness 70 %% = the log's c1.a (%.4f)", cx.k);
        PxmOld(cells, 2, -0.5f, -0.5f, 1, PxmComp, &cx, 0xFFFFFFFFuL, 1, old);
        for (k = 0; k < SH_PX_N; ++k) {
            const float d = fabsf((1.0f - (float)((old[k] >> 8) & 0xFFu) / 255.0f) - kLog[j][k]);
            if (d > worst) worst = d;
            g[k] = (unsigned char)(old[k] >> 8);
        }
        CHECK(ShPxVerdict(cx.k, g, a, &off) == SHX_OFF, "v3.46's pattern half a texel off: FAILED as logged (%d)", j);
        PxmOld(cells, -1, -0.5f, -0.5f, 1, PxmComp, &cx, 0xFFFFFFFFuL, 1, old);
        for (k = 2; k < SH_PX_N; ++k) {
            const float d = fabsf((1.0f - (float)((old[k] >> 8) & 0xFFu) / 255.0f) - kLog[j][k]);
            if (d > keep) keep = d;
        }
    }
    CHECK(worst <= 0.003f, "the v3.46 log = the composite's sim on mixed cells, the third Clear lost (worst %.4f)", worst);
    CHECK(keep > 0.05f, "with the third Clear in place the log is not reproduced (cells 2 / 3 off by %.3f)", keep);
    for (j = 0; j < 2; ++j)  // (2)
        for (bx = 0; bx < 2; ++bx)
            for (oi = -4; oi <= 4; ++oi)
                for (oj = -2; oj <= 2; ++oj) {
                    const float ox = 0.25f * (float)oi, oy = 0.25f * (float)oj;
                    float m;
                    cx.k = ShCompC1(kTf[j], 70 - 45 * j);
                    PxmFill(&t, PX_W, PX_H, PX_CW, cells, PX_CELLS, -1);
                    PxmProbe(&t, ox, oy, bx, PxmComp, &cx, 0xFFFFFFFFuL, 1, &p);
                    for (k = 0; k < SH_PX_N; ++k) g[k] = (unsigned char)(p.dst[k][PXC_MID] >> 8);
                    CHECK(ShPxVerdict(cx.k, g, a, &off) == SHX_OK, "v3.47 pattern ok at (%.2f, %.2f) %s (%.3f off)", ox, oy,
                          bx ? "black" : "clamp", off);
                    for (k = 0; k < PX_CELLS; ++k) CHECK(PxSrcHeld(cells[k], p.src[k], 0xFF000000uL), "the source as asked (%d)", k);
                    m = PxEdgeOffset(&p, 8, &n);
                    CHECK(n >= 2 && fabsf(m - ox) <= 0.2f, "the offset measured %.3f for %.2f (%d edges)", m, ox, n);
                    if (fabsf(m - ox) > wo) wo = fabsf(m - ox);
                    ++cases;
                }
    printf("composite pixel check (v3.47): the v3.46 log reproduced within %.4f (the third Clear lost); the new pattern ok in "
           "%d cases, the offset measured within %.3f texel\n", worst, cases, wo);
    cx.k = ShCompC1(0x4B, 70);  // (3)
    PxmFill(&t, PX_W, PX_H, PX_CW, cells, PX_CELLS, 2);
    PxmProbe(&t, -0.5f, -0.5f, 1, PxmComp, &cx, 0xFFFFFFFFuL, 1, &p);
    for (k = 0; k < SH_PX_N; ++k) g[k] = (unsigned char)(p.dst[k][PXC_MID] >> 8), tr[k] = (float)PxByte(p.src[k], 24) / 255.0f;
    CHECK(!PxSrcHeld(cells[2], p.src[2], 0xFF000000uL) && PxSrcHeld(cells[1], p.src[1], 0xFF000000uL), "a lost Clear: found in the source");
    CHECK(ShPxVerdict(cx.k, g, a, &off) == SHX_OFF, "a lost Clear: against the coverage asked, FAILED");
    CHECK(ShPxVerdictT(cx.k, g, tr, a, &off) == SHX_OK, "a lost Clear: on the source as read back, ok (%.3f)", off);
    for (k = 0; k < SH_PX_N; ++k) g[k] = (unsigned char)(255 - kShPxA[k]);
    ShPxAsked(tr);
    CHECK(ShPxVerdictT(cx.k, g, tr, a, &off) == SHX_COVER, "ShPxVerdictT on the coverage asked = ShPxVerdict (COVER)");
}

// v3.49 SH349: the Tremer's wall shadow (a subtractive ribbon, Cd - Cs x As) at the Darkness: the alpha byte scaled
static void TestRibbon349(void)
{
    int k, a8, pct, worst = 0;
    unsigned c;
    // 100 % (and above): the game's colour untouched, every alpha
    for (a8 = 0; a8 <= 255; ++a8) {
        c = (unsigned)a8 << 24 | 0x808080u;
        CHECK(ShRibbonColour(c, 100) == c && ShRibbonColour(c, 150) == c, "100 %%: untouched %08X", c);
        CHECK(ShRibbonColour(c, 0) == 0x808080u && ShRibbonColour(c, -5) == 0x808080u, "0 %%: no darkening %08X", c);
    }
    // the stock 0x30 at the row's values: the share kept within half a byte, rgb never touched
    for (k = 0; k < SH_DARK_N; ++k) {
        const unsigned out = ShRibbonColour(0x30808080u, kShDarkPct[k]);
        const float share = ShRibbonDark(out >> 24) / ShRibbonDark(0x30u), want = ShDarkD(kShDarkPct[k]);
        CHECK((out & 0xFFFFFFu) == 0x808080u, "rgb kept at %d %%", kShDarkPct[k]);
        CHECK(fabsf(share - want) <= 0.5f / 48.0f + 1e-6f, "0x30 at %d %%: share %.3f (want %.3f)", kShDarkPct[k], share, want);
    }
    CHECK(ShRibbonAlpha(0x30u, 85) == 0x29u && ShRibbonAlpha(0x30u, 70) == 0x22u && ShRibbonAlpha(0x30u, 55) == 0x1Au &&
          ShRibbonAlpha(0x30u, 25) == 0x0Cu, "0x30: 85 %% 29, 70 %% 22, 55 %% 1A, 25 %% 0C (%02X %02X %02X %02X)", ShRibbonAlpha(0x30u, 85),
          ShRibbonAlpha(0x30u, 70), ShRibbonAlpha(0x30u, 55), ShRibbonAlpha(0x30u, 25));
    // monotonic in both the alpha and the percent; never darker than the game's; the fade (0x30 - i, i = 0..47) keeps order
    for (pct = 0; pct <= 100; ++pct)
        for (a8 = 0; a8 <= 255; ++a8) {
            const unsigned o = ShRibbonAlpha((unsigned)a8, pct);
            const int err = abs((int)o * 100 - a8 * pct);
            CHECK(o <= (unsigned)a8, "never darker: a8 %d at %d %%", a8, pct);
            if (a8) CHECK(ShRibbonAlpha((unsigned)a8 - 1u, pct) <= o, "monotonic in alpha %d at %d %%", a8, pct);
            if (pct) CHECK(ShRibbonAlpha((unsigned)a8, pct - 1) <= o, "monotonic in percent %d at a8 %d", pct, a8);
            CHECK(err <= 50, "rounding: a8 %d at %d %% -> %u", a8, pct, o);
            if (err > worst) worst = err;
        }
    CHECK(ShRibbonAlpha(300u, 100) == 255u, "a byte above 255 clamped");
    CHECK(ShRibbonKey(0x30u, 85) != ShRibbonKey(0x30u, 70) && ShRibbonKey(0x30u, 85) != ShRibbonKey(0u, 85), "the log key per value");
    printf("tremer wall shadow (v3.49): 0x30 -> %02X at 85 %%, %02X at 25 %%; rounding within %.2f of a byte\n", ShRibbonAlpha(0x30u, 85),
           ShRibbonAlpha(0x30u, 25), (float)worst / 100.0f);
}

// v3.52 SH352 (USER_FEEDBACK_v351 D5): the Darkness row, Left lighter / Right darker; old ini values read unchanged
static int StepModel(int v, int count, int left) { return left ? (v + count - 1) % count : (v + 1) % count; }  // sh4windowmod.c StepValue
static void TestDarkOrder352(void)
{
    static const int kOld[SH_DARK_N] = { 100, 85, 70, 55, 40, 25 };  // v3.43 .. v3.51
    int pct, k;
    for (pct = 0; pct <= 100; ++pct) {  // the ini holds a percentage: the same nearest row value as before, only its slot moved
        int best = 0, got;
        for (k = 1; k < SH_DARK_N; ++k)
            if (abs(kOld[k] - pct) < abs(kOld[best] - pct)) best = k;
        got = kShDarkPct[ShDarkIndex(pct)];
        CHECK(got == kOld[best], "v3.52: ini %d %% shows %d %% as before (shows %d)", pct, kOld[best], got);
        CHECK(pct < 25 || abs(got - pct) <= 7, "v3.52: ini %d %%: the nearest row value %d", pct, got);
    }
    for (k = 0; k < SH_DARK_N; ++k) {  // Right: more %, Left: less % (a wrap at the ends, as every Options row)
        const int r = StepModel(k, SH_DARK_N, 0), l = StepModel(k, SH_DARK_N, 1);
        if (k < SH_DARK_N - 1) CHECK(kShDarkPct[r] == kShDarkPct[k] + 15, "v3.52: Right from %d %% -> %d %%", kShDarkPct[k], kShDarkPct[r]);
        else CHECK(kShDarkPct[r] == 25, "v3.52: Right from 100 %% wraps to 25 %%");
        if (k > 0) CHECK(kShDarkPct[l] == kShDarkPct[k] - 15, "v3.52: Left from %d %% -> %d %%", kShDarkPct[k], kShDarkPct[l]);
        else CHECK(kShDarkPct[l] == 100, "v3.52: Left from 25 %% wraps to 100 %%");
        CHECK(ShCompC1(0x4B, kShDarkPct[k]) <= ShCompC1(0x4B, kShDarkPct[r]) || r == 0, "v3.52: Right never lightens (but the wrap)");
    }
    CHECK(kShDarkPct[StepModel(ShDarkIndex(85), SH_DARK_N, 0)] == 100 && kShDarkPct[StepModel(ShDarkIndex(85), SH_DARK_N, 1)] == 70,
          "v3.52: the default 85 %%: Right 100 %%, Left 70 %%");
    CHECK(kShDarkPct[StepModel(ShDarkIndex(90), SH_DARK_N, 0)] == 100 && kShDarkPct[StepModel(ShDarkIndex(60), SH_DARK_N, 1)] == 40,
          "v3.52: an ini value off the row steps from its nearest (90 -> 85 -> Right 100; 60 -> 55 -> Left 40)");
    printf("darkness row (v3.52): 25 / 40 / 55 / 70 / 85 / 100 %%, Left lighter, Right darker; 101 ini values read as before\n");
}

// v3.52 SH352 (USER_FEEDBACK_v351 D8): why a composite is not soft; the game's darkness table (ShStockTf, the decomp's
// 0x55F660) over every stage, room, cut-scene and Playable Eileen scale: only stage 4 room 0x1F and cut-scene 0x43 have
// no darkness, so with Edge Soft every other composite is soft
static void TestNs352(void)
{
    static const int kCut[] = { 0, 2, 9, 0x32, 0x43, 0x10 };
    static const float kPe[] = { 1.0f, 1.5f, 0.5f, 3.0f, 0.05f };
    static const unsigned kClear[] = { 0x00000000u, 0x80303030u, 0xFF8080FFu };
    ShNsRoom r;
    char t[SH_NS_TEXT_MAX], small[24];
    int st, room, c, k, q, nodark = 0, soft = 0, len;
    unsigned big;
    CHECK(SHN_N == 9 && SHN_N <= 32 && !ShNsHard(SHN_SOFT) && !ShNsHard(SHN_NODARK) && !ShNsHard(-1) && !ShNsHard(SHN_N), "v3.52: kinds");
    for (k = SHN_STOCK; k < SHN_N; ++k) CHECK(ShNsHard(k) && kShNsName[k][0] && kShNsWhy[k][0], "v3.52: %d is a hard edge, named", k);
    CHECK(ShNsGate(0, SH_SOFT, 0) == SHN_NODARK && ShNsGate(0, SH_STOCK, 1) == SHN_NODARK, "v3.52: no darkness first (nothing shows)");
    CHECK(ShNsGate(0x50, SH_STOCK, 0) == SHN_STOCK && ShNsGate(0x50, SH_STOCK, 1) == SHN_STOCK, "v3.52: Stock chosen");
    CHECK(ShNsGate(0x50, SH_SOFT, 1) == SHN_OFF && ShNsGate(0x50, SH_SOFT, 0) == SHN_SOFT && ShNsGate(1, SH_SOFT, 0) == SHN_SOFT,
          "v3.52: off this session / soft");
    for (st = 0; st <= 12; ++st)
        for (room = 0; room <= 0x40; ++room)
            for (c = 0; c < (int)(sizeof kCut / sizeof kCut[0]); ++c)
                for (k = 0; k < (int)(sizeof kPe / sizeof kPe[0]); ++k)
                    for (q = 0; q < (int)(sizeof kClear / sizeof kClear[0]); ++q) {
                        const unsigned tf = ShPeScale(ShStockTf(st, room, kClear[q], kCut[c]), kPe[k]);
                        const int g = ShNsGate(ShTfAlpha(tf), SH_SOFT, 0);
                        const int none = kCut[c] == 0x43 || (st == 4 && room == 0x1F && kCut[c] != 2 && kCut[c] != 9 && kCut[c] != 0x32);
                        CHECK(g == (none ? SHN_NODARK : SHN_SOFT), "v3.52: stage %d room %02X cut %02X pe %.2f: %s", st, room, kCut[c],
                              kPe[k], kShNsName[g]);
                        nodark += g == SHN_NODARK, soft += g == SHN_SOFT;
                    }
    CHECK(ShStockTf(4, 0x1F, 0, 0) == 0u && ShStockTf(4, 0x1F, 0, 9) == 0x78000000u && ShStockTf(4, 0x11, 0, 0) == 0x50000000u,
          "v3.52: 4:1F has no darkness (a cut-scene 9 there has 0x78)");
    // the room's counts and its line's tail
    ShNsReset(&r);
    CHECK(ShNsText(t, sizeof t, &r) == 0 && !t[0], "v3.52: nothing counted: no tail");
    for (k = 0; k < 1200; ++k) CHECK(!ShNsAdd(&r, SHN_SOFT), "v3.52: soft is never noted");
    CHECK(ShNsText(t, sizeof t, &r) == 0 && !t[0] && r.n[SHN_SOFT] == 1200, "v3.52: all soft: no tail (the line as before)");
    ShNsReset(&r);
    CHECK(ShNsAdd(&r, SHN_NODARK) == 1, "v3.52: the first no-darkness composite is noted");
    for (k = 1; k < 2769; ++k) CHECK(ShNsAdd(&r, SHN_NODARK) == 0, "v3.52: once a room");
    len = ShNsText(t, sizeof t, &r);
    CHECK(!strcmp(t, "; not soft: 0 hard, 2769 with no darkness (the game's own 0: nothing drawn)") && len == (int)strlen(t),
          "v3.52: room 4:1F's tail: '%s'", t);
    CHECK(ShNsAdd(&r, SHN_RT) == 1 && ShNsAdd(&r, SHN_RT) == 0 && ShNsAdd(&r, SHN_STOCK) == 1 && ShNsAdd(&r, 77) == 1 &&
          ShNsAdd(&r, -3) == 0 && r.n[SHN_FAIL] == 2, "v3.52: each reason once; a bad reason counts as failed");
    ShNsAdd(&r, SHN_SOFT);
    ShNsText(t, sizeof t, &r);
    CHECK(!strcmp(t, "; not soft: 5 hard (Stock 1, render targets 2, failed 2), 2769 with no darkness (the game's own 0: nothing drawn)") &&
          ShNsHardN(&r) == 5, "v3.52: mixed tail: '%s'", t);
    ShNsReset(&r);
    CHECK(r.seen == 0 && ShNsAdd(&r, SHN_RT) == 1, "v3.52: a new room notes again");
    for (k = 0; k < SHN_N; ++k) r.n[k] = 4294967295u;  // the longest tail fits, a short buffer truncates (no CRT abort)
    len = ShNsText(t, sizeof t, &r);
    big = (unsigned)strlen(t);
    CHECK(len == (int)big && big + 1 < SH_NS_TEXT_MAX && strstr(t, "failed 4294967295), 4294967295 with no darkness"),
          "v3.52: the longest tail %u bytes fits %d", big, SH_NS_TEXT_MAX);
    memset(small, 'x', sizeof small);
    len = ShNsText(small, 16, &r);
    CHECK(len <= 15 && small[15] == 0 && small[16] == 'x' && !strncmp(small, "; not soft: 429", 15), "v3.52: truncated to 15 (%d)", len);
    CHECK(ShNsText(NULL, 16, &r) == 0 && ShNsText(small, 1, &r) == 0, "v3.52: no buffer");
    printf("not-soft reasons (v3.52): %d stage / room / cut / PE / clear cases: %d soft, %d with no darkness (4:1F and cut-scene 0x43 "
           "only); the longest room tail %u bytes\n", soft + nodark, soft, nodark, big);
}

int main(void)
{
    TestDarkOrder352();  // v3.52 SH352
    TestNs352();  // v3.52 SH352
    TestRibbon349();  // v3.49 SH349
    TestPx347();  // v3.47 PX347
    TestParse();
    TestFilter1D();
    TestPlans();
    Test2D();
    TestDarkness();
    TestDark342();  // v3.42 SM342
    TestC1343();  // v3.43 SH343
    TestComp344();  // v3.44 CS344
    TestThin();
    TestHookAndPreview();
    TestFigure();
    printf("shadows_sim_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
