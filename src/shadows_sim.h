// v3.40 SH340: softer shadows, the pure logic (no Windows, no game memory; test\shadows_sim_test.c, tools\shadow_mockup.py
// mirrors the same numbers). shadows.inc does the device work.
//
// The stock composite (ShadowCompositeDraw 0x55F660, decomp src/model/shadow_55e610.c): the stencil left by the shadow
// volumes becomes a mask (0x12E7C40, back-buffer size, A8R8G8B8) that holds TEXTUREFACTOR (colour c, alpha a) where in
// shadow and 0 elsewhere; it is laid over the picture twice (offsets (0, 0) and (-2, -2) px, SRCALPHA / INVSRCALPHA), so
// inside: dst (1 - a)^2 + c (1 - (1 - a)^2), and a 2 px one-copy fringe on two sides: a hard, stepped edge whose width
// does not grow with the resolution.
//
// v3.41 SH341 (USER_FEEDBACK_v340: "one setting twice as soft as Softer that is not blocky; remove the old Soft and Softer
// and call the new one Soft"): Stock / Soft only; Soft = a 12 px edge at 720 lines (twice v3.40's Softer), x height / 720.
// v3.40 drew a box-filtered half / quarter mask back in ONE 2x / 4x bilinear stretch: the small picture's grid showed as
// blocks along every edge. Now (all draws 4 bilinear taps, one ps.1.1 shader each):
//   1. down in 2x steps to level L (1..4: half .. sixteenth size), each a 4 x 4 box (taps at +-1 source texel); the
//      first one also turns the mask's alpha a into coverage 0..1 (x 32 c, saturated; c = 1.01 / (32 a)): the chain
//      carries 255 levels, not a's 50, and the inside is exactly 1;
//   2. n Kawase passes at level L (taps at +-o texels diagonally; o = 0.5 (a 2x2 mean each) except the last, tuned in
//      [0, 1.5] to hit the width): a near-Gaussian blur at the small level;
//   3. back up in 2x steps to the half size (a tent: taps at +-0.5 source texel), then the composite onto the screen
//      from the half size with the same tent: no draw stretches more than 2x, so no grid shows;
//   4. the composite: T = the blurred coverage, A = K (T + T (1 - T)) (the "core" curve: thin parts keep more of their
//      darkness than a straight ramp would, the edge starts softly), K = stock's 1 - (1 - a)^2: inside (T = 1) exactly
//      stock's darkness, colour c (tinted stages 9-11 keep their colour).
// L and n are picked per resolution (ShMakePlan): the most levels that can still reach the asked width, then the most
// passes (the smallest last offset: closest to a Gaussian); the 10-90 % width of the darkening across a straight edge is
// measured on the 1D emulation of the exact draws (D3D9 sampling), averaged over the edge's place against the grid.
#ifndef SHADOWS_SIM_H
#define SHADOWS_SIM_H
#include <math.h>
#include <stdlib.h>  // v3.42 SM342: abs (ShDarkIndex)
#include <string.h>
#include <stdio.h>  // v3.52 SH352: _snprintf_s (ShNsText)
#include "post_sim.h"  // v3.44 CS344: the ps.1.1 tokens and PsLint (kShCompPs below)

enum { SH_STOCK, SH_SOFT, SH_EDGE_N };  // v3.41 SH341: Softer is gone (v3.40's Soft / Softer both read as the new Soft)
static const char *const kShEdgeName[SH_EDGE_N] = { "Stock", "Soft" };
#define SH_EDGE_IDS      3       // v3.41 SH341: the value / text id slots stay v3.40's three (Stock, Soft, Softer): never renumbered
#define SH_SOFT_PX720    12.0f   // v3.41 SH341: the new Soft = twice v3.40's Softer (6 px) at 720 lines, x height / 720
#define SH_PX720_MIN     2.0f    // hidden [Shadows] SoftPx is clamped to this range
#define SH_PX720_MAX     24.0f
#define SH_LEVELS_MAX    4       // half, quarter, eighth, sixteenth size
#define SH_KAWASE_MAX    4       // blur passes at the small level
#define SH_STEPS_MAX     (SH_LEVELS_MAX + SH_KAWASE_MAX + SH_LEVELS_MAX - 1 + 1)  // down + blur + up + the composite: 12 draws
#define SH_PASSES_MAX    SH_STEPS_MAX
#define SH_RT_MIN        8       // a level is at least this many texels each way
#define SH_OFF_LO        0.5f    // the Kawase offsets (level texels): 0.5 each (a 2 x 2 mean a tap), the last one in
#define SH_LAST_LO       0.0f    // [LAST_LO, OFF_HI]: 0 = no blur (n passes at 0 = n - 1 at 0.5: the ranges join up)
#define SH_OFF_HI        1.5f
#define SH_OFF_STEPS     12      // the width table: the last offset in steps of (HI - LAST_LO) / 12
#define SH_TENT          0.5f    // the up passes' and the composite's taps (+-0.5 source texel)
#define SH_NORM_GAIN     32.0f   // the first pass: mul_x4 by c, then add_sat r0, r0, r0 three times (x 2 each)
#define SH_STOCK_OFF     2.0f    // the stock copies' offset (0x699C08: (0, 0) and (-2, -2))
#define SH_WIDTH_TOL     0.03f   // a plan within 3 % of the asked width is not "clamped"
#define SH_KEEP_MARGIN_MB 384    // v3.41 SH341: the preview's / pause picture's targets are kept between pauses above this margin
// The composite's quad starts at x = -1 (u 0 at x -1, 1 at W - 1): pixel i reads the mask at i + 0.5 - 1 = the midpoint
// of the two stock copies (the game's quads, without the -0.5 shift, read it at i and at i + 2 - 0.5 ... ), so the soft
// edge sits where the stock edge's middle is.
#define SH_COMP_X0       (-1.0f)

// ---------------------------------------------------------------- settings
// "Stock" / "Soft" (any case) or 0 / 1; v3.40's "Softer" / 2 read as Soft (*wasSofter = 1); -1 = not a value
static int ShParseEdge(const char *s, int *wasSofter)
{
    static const char *const kNames[SH_EDGE_IDS] = { "Stock", "Soft", "Softer" };
    int k;
    if (wasSofter) *wasSofter = 0;
    if (!s || !*s) return -1;
    if (s[0] >= '0' && s[0] <= '2' && !s[1]) k = s[0] - '0';
    else {
        for (k = 0; k < SH_EDGE_IDS; ++k) {
            const char *a = s, *b = kNames[k];
            while (*a && *b && (*a | 0x20) == (*b | 0x20)) ++a, ++b;
            if (!*a && !*b) break;
        }
        if (k == SH_EDGE_IDS) return -1;
    }
    if (k == 2) {
        if (wasSofter) *wasSofter = 1;
        k = SH_SOFT;
    }
    return k;
}
static float ShClampPx(float px, float def)
{
    if (!(px == px) || px <= 0.0f) return def;
    return px < SH_PX720_MIN ? SH_PX720_MIN : px > SH_PX720_MAX ? SH_PX720_MAX : px;
}
// the asked 10-90 % width in pixels of this resolution (0 for Stock)
static float ShTargetPx(int edge, float softPx720, int height)
{
    const float k = height > 0 ? (float)height / 720.0f : 1.0f;
    return edge == SH_SOFT ? ShClampPx(softPx720, SH_SOFT_PX720) * k : 0.0f;
}

// ---------------------------------------------------------------- the GPU's sampling, in one dimension
// D3D9 rules (GOG's wrapper is D3D8 over D3D9; dgVoodoo emulates them): a pixel's centre is at its integer coordinate, a
// texel k of an n-texel texture is centred at u = (k + 0.5) / n, bilinear between the two nearest centres, CLAMP. A 2D
// draw with 4 diagonal taps (+-du, +-dv) is the product of two 1D 2-tap means, so one dimension describes it exactly.
static float ShTap(const float *t, int n, float u)
{
    const float s = u * (float)n - 0.5f;
    const float fl = floorf(s), f = s - fl;
    int i0 = (int)fl, i1 = i0 + 1;
    if (i0 < 0) i0 = 0;
    if (i0 > n - 1) i0 = n - 1;
    if (i1 < 0) i1 = 0;
    if (i1 > n - 1) i1 = n - 1;
    return t[i0] * (1.0f - f) + t[i1] * f;
}
static int ShHalf(int n) { return (n + 1) / 2; }
// one down pass (v3.40's, kept for the tests): target texel j = the mean of two taps at its centre -+ 1 source texel
static int ShDown1D(const float *src, int n, float *dst)
{
    const int m = ShHalf(n);
    int j;
    for (j = 0; j < m; ++j) {
        const float u = ((float)j + 0.5f) / (float)m;
        dst[j] = 0.5f * (ShTap(src, n, u - 1.0f / (float)n) + ShTap(src, n, u + 1.0f / (float)n));
    }
    return m;
}
// the core curve (the composite: mad r1, r0, 1-r0, r0): 0 -> 0, 1 -> 1, slope 2 at the outside, 0 at the inside
static float ShCore(float t) { return t + t * (1.0f - t); }

// ---------------------------------------------------------------- the chain's draws
enum { SHS_NORM, SHS_DOWN, SHS_KAWASE, SHS_UP, SHS_COMP, SHS_N };  // NORM = the first down pass (alpha -> coverage)
static const char *const kShStepName[SHS_N] = { "down (coverage)", "down", "blur", "up", "composite" };
enum { SHB_MASK, SHB_L1, SHB_L2, SHB_L3, SHB_L4, SHB_X, SHB_N };   // the game's mask, our level 1..4 targets, a second
                                                                    // target at level L (the blur passes ping-pong)
typedef struct {
    int kind, src, dst;
    int sw, sh, dw, dh;  // the source's and the target's size (the composite's target: the screen)
    float du, dv;        // the 4 taps at +-du, +-dv (UV of the source)
} ShStep;
typedef struct {
    int edge, levels, kawase;              // levels 0 = Stock (nothing of ours)
    float off[SH_KAWASE_MAX];              // the blur passes' offsets (level-L texels)
    float target, width;                   // the asked and the reached 10-90 % width (px)
    int clamped;                           // 1: not within SH_WIDTH_TOL of the asked width (the nearest the chain reaches)
    int w[SH_LEVELS_MAX + 1], h[SH_LEVELS_MAX + 1];  // [0] the mask / screen, [k] level k
    int nsteps;
    ShStep step[SH_STEPS_MAX];
} ShPlan;
static int ShBufW(const ShPlan *p, int b) { return b == SHB_X ? p->w[p->levels] : p->w[b]; }
static int ShBufH(const ShPlan *p, int b) { return b == SHB_X ? p->h[p->levels] : p->h[b]; }
static void ShAddStep(ShPlan *p, int kind, int src, int dst, float tap)
{
    ShStep *s = &p->step[p->nsteps++];
    s->kind = kind, s->src = src, s->dst = dst;
    s->sw = ShBufW(p, src), s->sh = ShBufH(p, src);
    s->dw = kind == SHS_COMP ? p->w[0] : ShBufW(p, dst), s->dh = kind == SHS_COMP ? p->h[0] : ShBufH(p, dst);
    s->du = tap / (float)s->sw, s->dv = tap / (float)s->sh;
}
// the draws of a plan (levels, kawase, off, w, h set): down 1..L, the blur at L (ping-pong L <-> X), up L -> 1, the composite
static void ShBuildSteps(ShPlan *p)
{
    int k, cur;
    p->nsteps = 0;
    for (k = 1; k <= p->levels; ++k) ShAddStep(p, k == 1 ? SHS_NORM : SHS_DOWN, k - 1, k, 1.0f);
    cur = p->levels;
    for (k = 0; k < p->kawase; ++k) {
        const int dst = cur == SHB_X ? p->levels : SHB_X;
        ShAddStep(p, SHS_KAWASE, cur, dst, p->off[k]);
        cur = dst;
    }
    for (k = p->levels; k >= 2; --k) {
        ShAddStep(p, SHS_UP, cur, k - 1, SH_TENT);
        cur = k - 1;
    }
    ShAddStep(p, SHS_COMP, cur, -1, SH_TENT);
}
// the buffers a plan draws into (bit b = buffer b): the render targets to make
static unsigned ShPlanBufs(const ShPlan *p)
{
    unsigned m = 0;
    int k;
    for (k = 0; k < p->nsteps; ++k)
        if (p->step[k].kind != SHS_COMP) m |= 1u << p->step[k].dst;
    return m;
}

// ---------------------------------------------------------------- the 1D emulation of a plan's draws
#define SH_STRIP 512  // the 1D test strip (an edge in the middle; 32 texels at the sixteenth size)
// one draw in 1D: target texel j (centre u = (j + 0.5) / dw; the composite's quad from x = X0: u = (i - X0) / dw)
static void ShStep1D(const ShStep *s, const float *src, float *dst)
{
    int j;
    for (j = 0; j < s->dw; ++j) {
        const float u = s->kind == SHS_COMP ? ((float)j - SH_COMP_X0) / (float)s->dw : ((float)j + 0.5f) / (float)s->dw;
        dst[j] = 0.5f * (ShTap(src, s->sw, u - s->du) + ShTap(src, s->sw, u + s->du));
    }
}
// the darkening profile (share of stock's inside darkness, 0..1) across `in` (coverage 0..1 on the strip); 1D plan of
// the same levels / passes / offsets on a strip of n pixels
static void ShRun1D(int levels, int kawase, const float *off, const float *in, int n, float *out)
{
    static float buf[SHB_N][SH_STRIP * 2];
    ShPlan p;
    int k, i;
    memset(&p, 0, sizeof p);
    p.levels = levels, p.kawase = kawase;
    for (k = 0; k < kawase; ++k) p.off[k] = off[k];
    p.w[0] = p.h[0] = n;
    for (k = 1; k <= levels; ++k) p.w[k] = p.h[k] = ShHalf(p.w[k - 1]);
    ShBuildSteps(&p);
    memcpy(buf[SHB_MASK], in, sizeof(float) * (size_t)n);
    for (k = 0; k < p.nsteps; ++k) {
        const ShStep *s = &p.step[k];
        ShStep1D(s, buf[s->src], s->kind == SHS_COMP ? out : buf[s->dst]);
    }
    for (i = 0; i < n; ++i) out[i] = ShCore(out[i]);
}
// the 10-90 % width (pixels) of a rising profile p[0..n) from its minimum to its maximum (linear between samples)
static float ShWidth1090(const float *p, int n)
{
    float lo = p[0], hi = p[0], t10, t90, x10 = -1.0f, x90 = -1.0f;
    int i;
    for (i = 1; i < n; ++i) {
        if (p[i] < lo) lo = p[i];
        if (p[i] > hi) hi = p[i];
    }
    if (!(hi - lo > 1e-6f)) return 0.0f;
    t10 = lo + 0.1f * (hi - lo), t90 = lo + 0.9f * (hi - lo);
    for (i = 1; i < n; ++i) {
        if (x10 < 0.0f && p[i - 1] < t10 && p[i] >= t10) x10 = (float)(i - 1) + (t10 - p[i - 1]) / (p[i] - p[i - 1]);
        if (x90 < 0.0f && p[i - 1] < t90 && p[i] >= t90) x90 = (float)(i - 1) + (t90 - p[i - 1]) / (p[i] - p[i - 1]);
    }
    return x10 >= 0.0f && x90 >= x10 ? x90 - x10 : 0.0f;
}
// a step edge (0 left of `edge`, 1 from it on) through the chain
static void ShEdgeProfile(int levels, int kawase, const float *off, int edge, float *out)
{
    static float m[SH_STRIP];
    int i;
    for (i = 0; i < SH_STRIP; ++i) m[i] = i >= edge ? 1.0f : 0.0f;
    ShRun1D(levels, kawase, off, m, SH_STRIP, out);
}
// the width of a straight edge through the chain, averaged over 4 places against the level-L grid (2^L pixels a texel)
static float ShChainWidth(int levels, int kawase, const float *off)
{
    static float p[SH_STRIP];
    const int period = 1 << levels, step = period >= 4 ? period / 4 : 1, n = period >= 4 ? 4 : period;
    float sum = 0.0f;
    int k;
    for (k = 0; k < n; ++k) {
        ShEdgeProfile(levels, kawase, off, SH_STRIP / 2 + k * step, p);
        sum += ShWidth1090(p, SH_STRIP);
    }
    return sum / (float)n;
}
// the offsets of n passes with the last one o
static void ShOffsets(int kawase, float last, float *off)
{
    int k;
    for (k = 0; k < kawase; ++k) off[k] = k == kawase - 1 ? last : SH_OFF_LO;
}
// the width table (made once: ShPlanInit at install, else at the first plan): [levels][passes][last offset step]
static float g_shWTab[SH_LEVELS_MAX + 1][SH_KAWASE_MAX + 1][SH_OFF_STEPS + 1];
static int g_shWTabOk;
static float ShOffAt(int i) { return SH_LAST_LO + (SH_OFF_HI - SH_LAST_LO) * (float)i / (float)SH_OFF_STEPS; }
static void ShPlanInit(void)
{
    float off[SH_KAWASE_MAX];
    int L, n, i;
    if (g_shWTabOk) return;
    for (L = 1; L <= SH_LEVELS_MAX; ++L)
        for (n = 0; n <= SH_KAWASE_MAX; ++n)
            for (i = 0; i <= SH_OFF_STEPS; ++i) {
                if (n == 0 && i > 0) {
                    g_shWTab[L][n][i] = g_shWTab[L][n][0];
                    continue;
                }
                ShOffsets(n, ShOffAt(i), off);
                g_shWTab[L][n][i] = ShChainWidth(L, n, off);
            }
    g_shWTabOk = 1;
}

// ---------------------------------------------------------------- the plan for a resolution
static void ShPlanSet(ShPlan *p, int L, int n, float last)
{
    p->levels = L, p->kawase = n;
    memset(p->off, 0, sizeof p->off);
    ShOffsets(n, last, p->off);
}
// the last offset in [ShOffAt(from), HI] that gives `target` with (L, n), by the table (linear between its steps); -1 =
// out of reach
#define SH_STEP_HALF ((int)((SH_OFF_LO - SH_LAST_LO) * SH_OFF_STEPS / (SH_OFF_HI - SH_LAST_LO) + 0.5f))  // the step of 0.5
static float ShSolveOff(int L, int n, float target, int from)
{
    const float *t = g_shWTab[L][n];
    int i;
    if (n == 0) return fabsf(t[0] - target) <= SH_WIDTH_TOL * target ? SH_OFF_LO : -1.0f;
    for (i = from; i < SH_OFF_STEPS; ++i) {
        const float a = t[i], b = t[i + 1];
        if (target >= a && target <= b && b > a) return ShOffAt(i) + (ShOffAt(i + 1) - ShOffAt(i)) * (target - a) / (b - a);
    }
    return -1.0f;
}
// 1 = a plan (Soft); 0 = Stock or a size that cannot hold a chain
static int ShMakePlan(int edge, int w, int h, float softPx720, ShPlan *p)
{
    int k, L, n, Lavail = 0, done = 0;
    memset(p, 0, sizeof *p);
    p->edge = edge;
    if (edge != SH_SOFT || w < 2 * SH_RT_MIN || h < 2 * SH_RT_MIN) return 0;
    ShPlanInit();
    p->w[0] = w, p->h[0] = h;
    for (k = 1; k <= SH_LEVELS_MAX; ++k) {
        p->w[k] = ShHalf(p->w[k - 1]), p->h[k] = ShHalf(p->h[k - 1]);
        if (p->w[k] >= SH_RT_MIN && p->h[k] >= SH_RT_MIN && Lavail == k - 1) Lavail = k;
    }
    if (Lavail < 1) return 0;
    p->target = ShTargetPx(edge, softPx720, h);
    // the most levels that reach the width (the least fill; the blur at that level then spans about 1.4 texels or more,
    // so its grid does not show), then the most passes (the smallest last offset: the closest to a Gaussian); first with
    // a last offset of 0.5 .. 1.5 (a full pass), then from 0 (a lighter last pass fills the gaps between those ranges)
    {
        int tier;
        for (tier = 0; tier < 2 && !done; ++tier)
            for (L = Lavail; L >= 1 && !done; --L)
                for (n = SH_KAWASE_MAX; n >= 0 && !done; --n) {
                    const float o = ShSolveOff(L, n, p->target, tier ? 0 : SH_STEP_HALF);
                    if (o >= 0.0f) ShPlanSet(p, L, n, o), done = 1;
                }
    }
    if (!done) {  // out of reach: the nearest end of any range (more levels, then fewer passes on a tie)
        float best = 1e9f;
        for (L = Lavail; L >= 1; --L)
            for (n = 0; n <= SH_KAWASE_MAX; ++n) {
                const int ends[2] = { 0, SH_OFF_STEPS };
                int e;
                for (e = 0; e < 2; ++e) {
                    const float err = fabsf(g_shWTab[L][n][ends[e]] - p->target);
                    if (err < best - 0.01f) best = err, ShPlanSet(p, L, n, ShOffAt(ends[e]));
                }
            }
    }
    ShBuildSteps(p);
    p->width = ShChainWidth(p->levels, p->kawase, p->off);
    p->clamped = fabsf(p->width - p->target) > SH_WIDTH_TOL * p->target;
    return 1;
}
// video memory of the chain's targets (A8R8G8B8) / of the preview's two full-size copies / of one full-size picture
static unsigned ShChainBytes(const ShPlan *p)
{
    const unsigned m = ShPlanBufs(p);
    unsigned s = 0;
    int b;
    for (b = SHB_L1; b < SHB_N; ++b)
        if (m & (1u << b)) s += (unsigned)ShBufW(p, b) * (unsigned)ShBufH(p, b) * 4u;
    return s;
}
static unsigned ShCopyBytes(int w, int h) { return w > 0 && h > 0 ? 2u * (unsigned)w * (unsigned)h * 4u : 0u; }
static unsigned ShPicBytes(int w, int h) { return w > 0 && h > 0 ? (unsigned)w * (unsigned)h * 4u : 0u; }
// the largest stretch of any draw (target / source size, each way): 2 at most by design
static float ShMaxStretch(const ShPlan *p)
{
    float m = 0.0f;
    int k;
    for (k = 0; k < p->nsteps; ++k) {
        const ShStep *s = &p->step[k];
        const float sx = (float)s->dw / (float)s->sw, sy = (float)s->dh / (float)s->sh;
        if (sx > m) m = sx;
        if (sy > m) m = sy;
    }
    return m;
}

// ---------------------------------------------------------------- darkness
// Stock inside a shadow: dst (1 - a)^2 + c (1 - (1 - a)^2). Ours: one draw, SRCALPHA / INVSRCALPHA, colour c, alpha
// A = K core(T), T = the blurred coverage (1 inside), K = 1 - (1 - a)^2 / core(T inside): the same picture inside.
static float ShStockAlpha(float a) { return 1.0f - (1.0f - a) * (1.0f - a); }
// the first pass's constant c (alpha of c1): 32 c a = 1.01, so the inside saturates to exactly 1; at most 1 (a constant)
static float ShNormC(float a)
{
    const float c = a > 0.0f ? 1.01f / (SH_NORM_GAIN * a) : 1.0f;
    return c > 1.0f ? 1.0f : c;
}
static float ShNormT(float x, float c)  // the first pass on a mask alpha x: sat(32 c x)
{
    const float t = SH_NORM_GAIN * c * x;
    return t > 1.0f ? 1.0f : t < 0.0f ? 0.0f : t;
}
static float ShCompK(float a)  // the composite's K (alpha of c1): stock's inside alpha over core(the inside's coverage)
{
    const float tin = ShNormT(a, ShNormC(a)), g = ShCore(tin);
    return g > 0.0f ? ShStockAlpha(a) / g : 0.0f;
}
static float ShSoftAlphaT(float a, float t) { return ShCompK(a) * ShCore(t); }  // t = the blurred coverage
static float ShStockPixel(float dst, float c, float a, float cov1, float cov2)  // cov = the two copies' bilinear coverage
{
    const float a1 = a * cov1, a2 = a * cov2;
    dst = dst * (1.0f - a1) + c * a1;
    return dst * (1.0f - a2) + c * a2;
}
static float ShSoftPixelT(float dst, float c, float a, float t)
{
    const float A = ShSoftAlphaT(a, t);
    return dst * (1.0f - A) + c * A;
}
static int ShTfAlpha(unsigned tf) { return (int)(tf >> 24); }
static void ShTfColour(unsigned tf, float c[4])  // the composite's c2 = (r, g, b, 0)
{
    c[0] = (float)((tf >> 16) & 0xFF) / 255.0f;
    c[1] = (float)((tf >> 8) & 0xFF) / 255.0f;
    c[2] = (float)(tf & 0xFF) / 255.0f;
    c[3] = 0.0f;
}
// a replica of the stock darkness table (ShadowCompositeDraw's switch; c = the clear colour [0x13C5CBC]), for the tests
// and the mock-ups only: the hook reads the live TEXTUREFACTOR instead
static unsigned ShHalfColour(unsigned c, unsigned a)
{
    return ((((c >> 24) | a) & 0xFE) << 23) | (((c >> 16) & 0xFE) << 15) | (((c >> 8) & 0xFE) << 7) | ((c & 0xFE) >> 1);
}
static unsigned ShStockTf(int stage, int room, unsigned clear, int cut)
{
    unsigned tf;
    switch (stage) {
    case 1: case 2: tf = 0x32000000u; break;
    case 3: case 5: tf = 0x3C000000u; break;
    case 4: tf = room == 0x1F ? 0u : 0x50000000u; break;
    case 6: case 7: tf = 0x34000000u; break;
    case 9: case 10: tf = ShHalfColour(clear, 0x5A); break;  // 9 falls through into 10 (both rooms end there)
    case 11: tf = ShHalfColour(clear, 0x28); break;
    default: tf = 0x2D000000u; break;
    }
    if (cut == 2 || cut == 0x32) tf = 0x14000000u;
    if (cut == 9) tf = 0x78000000u;
    if (cut == 0x43) tf = 0u;
    return tf;
}
static unsigned ShPeScale(unsigned tf, float k)  // Playable Eileen's [Lighting] ShadowDarkness at 0x55FC56 (k = pct / 100)
{
    float a = (float)(tf >> 24) * k;
    a = a < 0.0f ? 0.0f : a > 255.0f ? 255.0f : a;
    return (tf & 0x00FFFFFFu) | (unsigned)(a + 0.5f) << 24;
}

// ---------------------------------------------------------------- v3.42 SM342: Darkness (the Shadows page's second row)
// USER_FEEDBACK_v341: "Soft shadow looks good now, but it's pitch black. Could you lighten that up?" The user's choice:
// a Darkness row, 100 / 85 / 70 / 55 % (v3.43 SH343: + 40 / 25 %), default 85 %, on top of Playable Eileen's [Lighting] ShadowDarkness (which
// scales TEXTUREFACTOR's alpha at 0x55FC56, before the mask pass: untouched), for Stock and Soft alike. d = pct / 100 is
// the share of the frame's inside darkness that is kept: the inside of a shadow is dst (1 - A) + c A with A = d x
// stock's own A (1 - (1 - a)^2, a = the frame's TEXTUREFACTOR alpha, PE's scaling included), the colour c unchanged
// (tinted stages 9-11 keep their tint; it only blends in less).
//   Soft: the composite's K (c1.a) x d: exactly d x stock's inside, the soft edge's shape the same.
//   Stock: the game's own two quads, stage 0's alpha argument 1 DIFFUSE (0xFFFFFFFF) -> TFACTOR with an alpha byte s8 for
//          that draw only (TEXTUREFACTOR put back right after): each copy blends a x s8 / 255, so the inside is
//          1 - (1 - a s)^2 = d (1 - (1 - a)^2) for s = (1 - sqrt(1 - d K)) / a. The one-copy 2 px fringe scales by about s.
// [Shadows] Darkness is a percentage 0..100 (the row's four values, any other one from the ini is used as it is and the
// row shows the nearest; "85", "85%", "85 %"); not a number: 85.
// v3.52 SH352 (USER_FEEDBACK_v351 D5: "Darkness scrolls the wrong way"): the row runs 25 -> 100 %, so Left is lighter and
// Right darker (StepValue: Right = the next index). The ini keeps the percentage, so every old value reads unchanged; the
// value texts are made from this table at start (ShTexts), so the text ids keep their slots.
#define SH_DARK_N   6  // v3.43 SH343: + 40 and 25 % (the host / the user: 55 % still dark in play); ids appended
#define SH_DARK_DEF 85
static const int kShDarkPct[SH_DARK_N] = { 25, 40, 55, 70, 85, 100 };  // v3.52 SH352: lightest first (was 100 .. 25)
static int ShDarkIndex(int pct)  // the row's value nearest pct (a tie: the lighter one, the first in the row)
{
    int k, best = 0;
    for (k = 1; k < SH_DARK_N; ++k)
        if (abs(kShDarkPct[k] - pct) < abs(kShDarkPct[best] - pct)) best = k;
    return best;
}
static int ShParseDark(const char *s, int *pct)  // 1 = a number (*pct clamped to 0..100), 0 = not one (*pct = SH_DARK_DEF)
{
    int v = 0, n = 0;
    *pct = SH_DARK_DEF;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t') ++s;
    for (; *s >= '0' && *s <= '9' && n < 6; ++s, ++n) v = v * 10 + (*s - '0');
    while (*s == ' ' || *s == '\t') ++s;
    if (*s == '%') ++s;
    while (*s == ' ' || *s == '\t') ++s;
    if (!n || *s) return 0;
    *pct = v > 100 ? 100 : v;
    return 1;
}
static float ShDarkD(int pct) { return pct <= 0 ? 0.0f : pct >= 100 ? 1.0f : (float)pct / 100.0f; }
// Soft: the composite's c1.a = stock's K x d
static float ShCompKDark(float a, float d) { return ShCompK(a) * (d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d); }
// Stock: the alpha byte (0..255) TFACTOR gives the game's two copies for an inside of d x stock's; 255 = no change
// (d = 1 or no darkness at all: the hook forwards the game's draw untouched)
static unsigned ShStockDarkByte(int tfAlpha, float d)
{
    float a, K, s;
    if (!(d < 0.9995f) || tfAlpha <= 0) return 255u;
    if (!(d > 0.0f)) return 0u;
    a = (float)(tfAlpha > 255 ? 255 : tfAlpha) / 255.0f;
    K = ShStockAlpha(a);
    s = (1.0f - sqrtf(1.0f - d * K)) / a;
    s = s * 255.0f + 0.5f;
    return s >= 255.0f ? 255u : s <= 0.0f ? 0u : (unsigned)s;
}
// the inside the GPU draws with that byte (the two copies, 8-bit factors: a8 x s8 / 255^2 each), for the tests
static float ShStockDarkInside(int tfAlpha, unsigned s8)
{
    const float a = (float)(tfAlpha > 255 ? 255 : tfAlpha) / 255.0f * (float)s8 / 255.0f;
    return ShStockAlpha(a);
}
// v3.43 SH343 (USER_FEEDBACK_v342 item 4: the Shadows page's right half lighter at 55 %, play "no visible change"): the
// in-play composite (ShDrawBody) and the page's right half (ShPreviewDraw) both take the composite's c1.a from this one
// function of (the frame's TEXTUREFACTOR alpha, the Darkness percent), so the two paths cannot differ; the hook reads
// c1 back from the device before its draw and logs what the draw used (ShC1Same: the upload arrived).
static float ShCompC1(int tfAlpha, int pct)
{
    const int a8 = tfAlpha < 0 ? 0 : tfAlpha > 255 ? 255 : tfAlpha;
    return ShCompKDark((float)a8 / 255.0f, ShDarkD(pct));
}
static int ShC1Same(float asked, float got) { return fabsf(asked - got) <= 0.5f / 255.0f + 1e-6f; }  // half an 8-bit step
// the share of the floor's light a shadow's inside lets through (1 - its alpha), for the log; c1 = the composite's c1.a
static float ShLightThrough(float c1) { return c1 < 0.0f ? 1.0f : c1 > 1.0f ? 0.0f : 1.0f - c1; }
// the hook's read-back: at a new (darkness alpha, percent) and every SH_C1_EVERY-th composite after (a silent check)
#define SH_C1_EVERY 1800
static unsigned ShC1Key(int tfAlpha, int pct) { return (unsigned)(tfAlpha & 0xFF) << 16 | (unsigned)(pct & 0xFFFF); }
static int ShC1Due(unsigned key, unsigned lastKey, unsigned n) { return key != lastKey || (n % SH_C1_EVERY) == 0; }

// v3.49 SH349 (USER_FEEDBACK_v348 E: "for soft shadows ... include the whole game"): the one shadow the game draws outside
// the stencil mask that ShDrawBody softens is the Tremer's (enemy type 9, the leech) wall shadow: a textured ribbon
// (EffectDrawRibbonShadow 0x45F480 -> EffectDrawRibbonFaded 0x45F1E0, its colour in eax) drawn with the PS2 blend 0x42
// = Cd - Cs x As (subtractive: (0 - Cs) As + Cd), As = the colour's alpha byte (stock 0x30, 0x30 - fade x 48 while it
// fades) x the depth fade. Its darkening is linear in As, so the Darkness keeps pct % of it by scaling the alpha byte:
// the same share as the stencil shadows' inside (ShCompC1 / ShStockDarkByte). 100 % = the byte untouched. Its edge is
// its texture's (no mask to blur): Soft does not change it.
static unsigned ShRibbonAlpha(unsigned a8, int pct)
{
    if (a8 > 255u) a8 = 255u;
    if (pct >= 100) return a8;
    if (pct <= 0) return 0u;
    return (a8 * (unsigned)pct + 50u) / 100u;
}
static unsigned ShRibbonColour(unsigned c, int pct) { return ShRibbonAlpha(c >> 24, pct) << 24 | (c & 0xFFFFFFu); }
// the darkening the ribbon's inside gives at the colour's alpha byte (rgb 0x80 = 1.0 on the PS2 scale, x the texture):
// As / 128 of the colour subtracted, for the log and the tests
static float ShRibbonDark(unsigned a8) { return (float)(a8 > 255u ? 255u : a8) / 128.0f; }
// a key per (alpha byte, percent) for the once-per-value line
static unsigned ShRibbonKey(unsigned a8, int pct) { return (a8 & 0xFFu) << 16 | ((unsigned)pct & 0xFFFFu); }

// v3.44 CS344 (USER_FEEDBACK_v343 item 1: the Darkness "does not work in preview or game"; post_sim.h's note): v3.41's
// composite core curve `mad r1, r0, 1-r0, r0` used the `1-x` source modifier, which GOG's translator drops silently: the
// draw ran as if no shader were set (FxBase's stage 0: alpha = the coverage texture's, colour black), so the inside was
// fully black at any Darkness. COMP rebuilt from proven forms only (PsLint), the same formula: T = the mean of 4 taps'
// coverage (the tent from the half size); out = (c, K (T + T (1 - T))), drawn SRCALPHA / INVSRCALPHA. c0 0.5,
// c1 (0, 0, 0, K) (K = ShCompC1), c2 (r, g, b, 0) of TEXTUREFACTOR. Every value stays in 0..1.
static const PsTok kShCompPs[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),         // r0 = (0, 0, 0, T)
    I_MUL, D_R(1), S_R(0), S_R(0),                 // T^2
    I_SUB, D_R(1), S_R(0), S_R(1),                 // T - T^2 = T (1 - T)
    I_ADD, D_R(1), S_R(1), S_R(0),                 // + T: the core curve
    I_MUL, D_R(1), S_R(1), S_C(1),                 // x K
    I_ADD, D_R(0), S_R(1), S_C(2),                 // + (c, 0)
    PS_END };
// the composite as the shader computes it on a coverage t (0..1) with K: alpha (the same as ShCompKDark x ShCore)
static float ShCompShaderA(float t, float k)
{
    const float t2 = t * t, c = (t - t2) + t;
    return c * k;
}
// The pixel check (shadows.inc ShPxCheck): a fixed test draw through the composite shader and the real c1: four source
// cells of coverage kShPxA (rgb 0), the colour c2 black, over a white target of ours, read back. The measured alpha of
// a cell = 1 - green / 255. SHX_COVER = the coverage itself came through (the fixed-function result: no shader).
#define SH_PX_N   4
#define SH_PX_TOL (3.0f / 255.0f)
static const unsigned char kShPxA[SH_PX_N] = { 255, 192, 128, 0 };
enum { SHX_OK, SHX_COVER, SHX_NOTHING, SHX_OFF, SHX_N };
static const char *const kShPxWhy[SHX_N] = { "the Darkness reaches the pixels",
                                             "the coverage itself came through: the composite shader did not run (no Darkness)",
                                             "nothing was drawn", "the pixels are not the shader's (its sim)" };
// v3.47 PX347: t = the source coverage the sim runs on (the source as read back; ShPxVerdict: the coverage asked)
static int ShPxVerdictT(float k, const unsigned char g[SH_PX_N], const float t[SH_PX_N], float a[SH_PX_N], float *maxOff)
{
    int i, cover = 1, none = 1;
    float off = 0.0f;
    for (i = 0; i < SH_PX_N; ++i) {
        const float d = fabsf((a[i] = 1.0f - (float)g[i] / 255.0f) - ShCompShaderA(t[i], k));
        if (d > off) off = d;
        if (fabsf(a[i] - t[i]) > SH_PX_TOL) cover = 0;
        if (g[i] < 254) none = 0;
    }
    if (maxOff) *maxOff = off;
    return off <= SH_PX_TOL ? SHX_OK : none ? SHX_NOTHING : cover ? SHX_COVER : SHX_OFF;
}
static void ShPxAsked(float t[SH_PX_N])
{
    int i;
    for (i = 0; i < SH_PX_N; ++i) t[i] = (float)kShPxA[i] / 255.0f;
}
static int ShPxVerdict(float k, const unsigned char g[SH_PX_N], float a[SH_PX_N], float *maxOff)
{
    float t[SH_PX_N];
    ShPxAsked(t);
    return ShPxVerdictT(k, g, t, a, maxOff);
}

// ---------------------------------------------------------------- the hook's own rules
// 0x55FE43 is DrawPrimitive(dev, TRIANGLELIST 4, 0, 4): the first two of the five offset quads. Anything else: stock.
static int ShCallOk(unsigned type, unsigned start, unsigned count) { return type == 4 && start == 0 && count == 4; }
// why the session went back to the stock draw (the first one is logged; the edge stays Stock until the next start)
enum { SHF_NONE, SHF_STATE, SHF_SHADER, SHF_RT, SHF_TARGET, SHF_DRAW, SHF_FAULT, SHF_N };
static const char *const kShFail[SHF_N] = { "", "a device state query failed", "a pixel shader could not be made",
                                            "a render target could not be made", "a render target could not be set",
                                            "a draw failed", "a fault" };

// ---------------------------------------------------------------- v3.52 SH352: why a composite is not soft (D8)
// USER_FEEDBACK_v351 D8: room 4:1F drew 0 soft composites of 85,000 while every other room was 100 % soft. The cause is
// the game's own darkness: ShadowCompositeDraw 0x55F660 (SH4Decomp src/model/shadow_55e610.c:278-281) sets
// TEXTUREFACTOR 0 for stage 4 room 0x1F (and cut-scene 0x43, :316), so the mask quad writes colour 0 alpha 0 over a mask
// cleared to 0, and the game's two copies blend with alpha 0: the picture is unchanged, no real-time shadow shows there,
// with Refined or without (Playable Eileen's ShadowDarkness scales 0 to 0; the pause copy logged darkness 00000000 there).
// ShDrawBody forwarded that empty draw, and the room line counted it as "not soft". Not a hard edge: nothing is drawn.
// Every composite now carries a reason; the room line says how many were hard and why, and how many had no darkness.
enum { SHN_SOFT, SHN_NODARK, SHN_STOCK, SHN_OFF, SHN_CALL, SHN_MASK, SHN_PLAN, SHN_RT, SHN_FAIL, SHN_N };
static const char *const kShNsName[SHN_N] = { "soft", "no darkness", "Stock", "off", "not the stock draw", "not the mask",
                                              "no plan", "render targets", "failed" };
static const char *const kShNsWhy[SHN_N] = {
    "soft",
    "the game's darkness is 0 here (TEXTUREFACTOR alpha 0: the mask is empty, the game's own draw leaves the picture as it is: no "
    "shadow shows, so there is nothing to soften)",
    "Edge Stock is chosen: the game's own hard edge",
    "soft shadows are off for this session (see the off line): the game's own hard edge",
    "the draw is not the stock one (4, 0, 4): the game's own hard edge",
    "stage 0 does not hold the shadow mask: the game's own hard edge",
    "no soft plan for the mask's size: the game's own hard edge",
    "our render targets could not be made this time: the game's own hard edge, soft again at the next composite",
    "the soft composite failed (soft shadows off from now on): the game's own hard edge" };
// a hard edge on screen (Stock chosen included); SHN_NODARK draws nothing, SHN_SOFT is soft
static int ShNsHard(int why) { return why > SHN_NODARK && why < SHN_N; }
// the gate before the soft path: SHN_SOFT = go on; no darkness first (nothing shows whatever the edge), then the chosen
// Stock, then the session's fallback
static int ShNsGate(int tfAlpha, int edge, int failed)
{
    return !tfAlpha ? SHN_NODARK : edge != SH_SOFT ? SHN_STOCK : failed ? SHN_OFF : SHN_SOFT;
}
typedef struct { unsigned n[SHN_N]; unsigned seen; } ShNsRoom;  // seen: a bit per reason already logged in this room
static void ShNsReset(ShNsRoom *r) { memset(r, 0, sizeof *r); }
// counts a composite; 1 = the reason's first non-soft composite in this room (its once-per-room line is due)
static int ShNsAdd(ShNsRoom *r, int why)
{
    if (why < 0 || why >= SHN_N) why = SHN_FAIL;
    ++r->n[why];
    if (why == SHN_SOFT || (r->seen & (1u << why))) return 0;
    r->seen |= 1u << why;
    return 1;
}
static unsigned ShNsHardN(const ShNsRoom *r)
{
    unsigned s = 0;
    int k;
    for (k = 0; k < SHN_N; ++k)
        if (ShNsHard(k)) s += r->n[k];
    return s;
}
// the room line's tail: "" when every composite was soft, else
// "; not soft: H hard (Stock 3, render targets 2), Z with no darkness (the game's own 0: nothing drawn)"
// (truncated, never the CRT's abort, when cap is short; SH_NS_TEXT_MAX holds the longest: every reason at 10 digits)
#define SH_NS_TEXT_MAX 256
static int ShNsText(char *o, size_t cap, const ShNsRoom *r)
{
    const unsigned hard = ShNsHardN(r);
    size_t len = 0;
    int k, a, first = 1;
    if (!o || cap < 2) return 0;
    o[0] = 0;
    if (!hard && !r->n[SHN_NODARK]) return 0;
#define SH_NS_PUT(...) do { if (len + 1 < cap) { a = _snprintf_s(o + len, cap - len, _TRUNCATE, __VA_ARGS__); len = a < 0 ? cap - 1 : len + (size_t)a; } } while (0)
    SH_NS_PUT("; not soft: %u hard", hard);
    for (k = 0; k < SHN_N; ++k)
        if (ShNsHard(k) && r->n[k]) {
            SH_NS_PUT("%s%s %u", first ? " (" : ", ", kShNsName[k], r->n[k]);
            first = 0;
        }
    if (!first) SH_NS_PUT(")");
    SH_NS_PUT(", %u with no darkness (the game's own 0: nothing drawn)", r->n[SHN_NODARK]);
#undef SH_NS_PUT
    return (int)len;
}

// ---------------------------------------------------------------- the preview (the Shadows page)
enum { SHP_NONE, SHP_SCENE, SHP_CARD };
enum { SHW_OFF, SHW_NOHOOK, SHW_FAILED, SHW_SCENE, SHW_TITLE, SHW_NOCOPY, SHW_STALE, SHW_N };
static const char *const kShWhy[SHW_N] = { "the preview is off ([Shadows] Preview=0)", "no page backdrop hook (screen effects not installed)",
                                           "soft shadows are off for this session", "the paused scene",
                                           "not opened from a paused game", "no copy of the paused scene (no shadow drawn in the pause's frame)",
                                           "the copy is not this pause's" };
typedef struct {
    int previewOn, backdropHook, failed;  // [Shadows] Preview, the 0x526A53 hook, the session's stock fallback
    int pausedGame;                       // gameplay paused ([0xFD5C20] == 2 in the game)
    int haveCopies, copiesFrame, pauseFrame;  // the copies and the frame they were taken in; the latest pause picture's frame
} ShPrevIn;
static int ShPreviewMode(const ShPrevIn *in, int *why)
{
    int w;
    if (!in->previewOn) w = SHW_OFF;
    else if (!in->backdropHook) w = SHW_NOHOOK;
    else if (in->failed) w = SHW_FAILED;
    else if (!in->pausedGame) w = SHW_TITLE;
    else if (!in->haveCopies) w = SHW_NOCOPY;
    else if (in->copiesFrame != in->pauseFrame) w = SHW_STALE;
    else w = SHW_SCENE;
    if (why) *why = w;
    return w == SHW_OFF || w == SHW_NOHOOK ? SHP_NONE : w == SHW_SCENE ? SHP_SCENE : SHP_CARD;
}
// once a frame (0x55AC29): do the copies' contents end? 1 = yes (*why: 1 the pause ended, 2 left the game, 3 replaced by a
// newer pause). v3.41 SH341: ending them no longer frees their render targets (ShKeepTargets says whether those stay)
typedef struct { int haveCopies, gameplay, phase, capture, copiesFrame, pauseFrame; } ShKeepIn;
static int ShCopiesFree(const ShKeepIn *in, int *why)
{
    int w = 0;
    if (!in->haveCopies) w = 0;
    else if (!in->gameplay) w = 2;
    else if (in->phase != 2 && in->capture == 0) w = 1;  // play (0) or a real-time cut-scene (1) again: no overlay
    else if (in->pauseFrame != in->copiesFrame && in->capture == 2) w = 3;
    if (why) *why = w;
    return w != 0;
}
static const char *const kShFreeWhy[4] = { "", "the pause ended", "the game left play", "a newer pause has no copy" };
// v3.41 SH341 (the pause hitch): the full-size targets of a pause (the preview's two copies, the Screen Effects page's
// pause picture) are made once and kept for the next pause while the 32-bit address space has room (marginMb = free MB);
// they are released before every device reset and forgotten on a new device either way
static int ShKeepTargets(int marginMb) { return marginMb >= SH_KEEP_MARGIN_MB; }

// ---------------------------------------------------------------- the sample card's figure (opened from the title)
// A standing figure's shadow as hard-edged triangles (the mask's own look), centred on cx, from `top` down `hgt`
// pixels: head (16-gon), neck, torso, two arms, two legs (thin enough to show the blur thinning them). xy: 2 floats a
// vertex, 3 vertices a triangle; returns the triangle count (at most SH_FIG_TRIS).
#define SH_FIG_TRIS 64
static int ShFigQuad(float *xy, int n, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
    float *o = xy + n * 6;
    o[0] = x0, o[1] = y0, o[2] = x1, o[3] = y1, o[4] = x2, o[5] = y2;
    o[6] = x0, o[7] = y0, o[8] = x2, o[9] = y2, o[10] = x3, o[11] = y3;
    return n + 2;
}
static int ShFigure(float cx, float top, float hgt, float *xy)
{
    const float u = hgt;
    int n = 0, k;
    {   // head: 16 triangles round (cx, top + 0.065 u), radius 0.065 u
        const float hx = cx, hy = top + 0.065f * u, r = 0.065f * u;
        for (k = 0; k < 16; ++k) {
            const float a0 = 6.2831853f * (float)k / 16.0f, a1 = 6.2831853f * (float)(k + 1) / 16.0f;
            float *o = xy + n * 6;
            o[0] = hx, o[1] = hy;
            o[2] = hx + r * cosf(a0), o[3] = hy + r * sinf(a0);
            o[4] = hx + r * cosf(a1), o[5] = hy + r * sinf(a1);
            ++n;
        }
    }
    n = ShFigQuad(xy, n, cx - 0.025f * u, top + 0.12f * u, cx + 0.025f * u, top + 0.12f * u, cx + 0.03f * u, top + 0.16f * u,
                  cx - 0.03f * u, top + 0.16f * u);                                                          // neck
    n = ShFigQuad(xy, n, cx - 0.13f * u, top + 0.16f * u, cx + 0.13f * u, top + 0.16f * u, cx + 0.10f * u, top + 0.54f * u,
                  cx - 0.10f * u, top + 0.54f * u);                                                          // torso
    n = ShFigQuad(xy, n, cx - 0.155f * u, top + 0.17f * u, cx - 0.115f * u, top + 0.18f * u, cx - 0.14f * u, top + 0.52f * u,
                  cx - 0.18f * u, top + 0.51f * u);                                                          // left arm
    n = ShFigQuad(xy, n, cx + 0.115f * u, top + 0.18f * u, cx + 0.155f * u, top + 0.17f * u, cx + 0.18f * u, top + 0.51f * u,
                  cx + 0.14f * u, top + 0.52f * u);                                                          // right arm
    n = ShFigQuad(xy, n, cx - 0.10f * u, top + 0.53f * u, cx - 0.012f * u, top + 0.53f * u, cx - 0.03f * u, top + 1.0f * u,
                  cx - 0.085f * u, top + 1.0f * u);                                                          // left leg
    n = ShFigQuad(xy, n, cx + 0.012f * u, top + 0.53f * u, cx + 0.10f * u, top + 0.53f * u, cx + 0.085f * u, top + 1.0f * u,
                  cx + 0.03f * u, top + 1.0f * u);                                                           // right leg
    return n;
}
#endif
