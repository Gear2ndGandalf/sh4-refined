// v3.59 TV359: Thermal Vision, the pure rules (no game memory, no Windows): included by src/screenfx.inc and by the native
// test test/thermal_sim_test.c. Plan: staging/v359_feedback/w3_thermal/PLAN.md (USER_FEEDBACK_v358: "For Thermal Vision
// do not allow us to see heat through walls and objects"; "improve on the foot prints for the player by making it shaped
// like either Eileen's or Henry's depending on which character we're playing").
//
// 1. Walls hide heat. In every room with lights the game's light grid (0x433F00, called at 0x4EFBDC in the render frame
//    0x4EFBC0, after the 3D pass and before the scene hook 0x4EFBFD) flushes full-screen strips through 0x564210 with GS
//    TEST 0x3000D (ZTE, ZTST ALWAYS) at GS z 10000 and without the "no z write" byte [0x137F978] the game's other effects
//    set around their flushes: the depth buffer then holds 0.000596 everywhere (a wall at view depth ~27 000), so the heat
//    redraw (depth test GREATEREQUAL) passed over every wall (KB: depthfix.inc, staging/v331_feedback/w3_depth). While
//    Thermal Vision is on, Refined sets [0x137F978] = 1 around that one call, as the game does for its own effects: the
//    grid's colours are the same (ZTST ALWAYS), the scene's depth stays, walls / doors / furniture hide the bodies behind
//    them. ThDepthWanted decides it per frame; ThPrintDepth says whether this frame's depth is the scene's.
// 2. Footprints shaped like the shoes of who walks: Henry's boots (one sole: the ball, the arch on the outer edge, the
//    heel), Eileen's heeled shoes (a narrow sole and a separate heel dot), the old oval for the walking monsters. A shape
//    is one or two parts that never overlap (a sole strip from heel to toe, or an egg fanned from its centre: warm middle,
//    cooler rim) in the print's own frame: a along the walk (+ = toward the toe), b across it (+ = away from the walking
//    line, so the right and left prints mirror).
//    Who the player is: the hip bones' spacing of the model drawn (rigid in every pose: Playable Eileen 24.1 u, vanilla
//    Henry 36.0 u; sh4-player-effects sizes.md), with a dead band; without bones Playable Eileen's own SH4PE_Enabled().
#ifndef THERMAL_SIM_H
#define THERMAL_SIM_H
#include <math.h>

// ---------------------------------------------------------------- 1. the depth (walls hide heat)
#define TH_GRID_FLAG_VA    0x137F978u  // GfxFlush2dBatch 0x564210: == 1 -> SetRenderState(ZWRITEENABLE, 0) after its GS test
#define TH_GRID_CALL_VA    0x4EFBDCu   // `call 433F00` (E8 1F 43 F4 FF) in 0x4EFBC0, behind `if (0x55DD80() && ![0xFD5C2C])`
#define TH_GRID_FN_VA      0x433F00u

// the grid draws without a depth write this frame: the switch on ([ScreenEffects] ThermalTrueDepth), the hook in, Thermal
// Vision's body heat live (the filter, gameplay or a real-time cut-scene: ThermHeatLive), the shader not failed
static int ThDepthWanted(int keyOn, int hookIn, int heatLive)
{
    return keyOn && hookIn && heatLive;
}
// the depth at the scene hook is the scene's own: the hook is in and the grid either did not draw this frame (a room
// without such lights: the scene's depth as drawn) or drew without a depth write (kept). 0 = not known (no hook) or
// overwritten: the footprints keep v3.58's no-depth draw (with the collision wall test)
static int ThPrintDepth(int hookIn, int gridRan, int gridKept)
{
    if (!hookIn) return 0;
    return !gridRan || gridKept;
}

// ---------------------------------------------------------------- 2. the footprints
enum { TPS_OVAL, TPS_BOOT, TPS_HEEL, TPS_N };  // the old oval (monsters), Henry's boot, Eileen's / Cynthia's heeled shoe
static const char *const kTpsName[TPS_N] = { "the old oval", "Henry's boots", "Eileen's heels" };

#define TP_HIPS_E        24.1f   // Playable Eileen's hip bones (5 / 2) apart
#define TP_HIPS_H        36.0f   // vanilla Henry's
#define TP_HIPS_TO_H     31.5f   // at or over this: Henry's boots ...
#define TP_HIPS_TO_E     28.5f   // ... at or under this: Eileen's heels; between: as before (a dead band)
// who is played: hips = the measured spacing (hipsOk 0: no usable bones), peOn = Playable Eileen's SH4PE_Enabled() (-1: no
// such export / not loaded), prev = the shape used so far (-1: none yet)
static int TpBodyShape(float hips, int hipsOk, int peOn, int prev)
{
    if (hipsOk && hips > 2.0f && hips < 200.0f) {
        if (hips >= TP_HIPS_TO_H) return TPS_BOOT;
        if (hips <= TP_HIPS_TO_E) return TPS_HEEL;
        if (prev == TPS_BOOT || prev == TPS_HEEL) return prev;
        return hips >= 0.5f * (TP_HIPS_E + TP_HIPS_H) ? TPS_BOOT : TPS_HEEL;
    }
    if (prev == TPS_BOOT || prev == TPS_HEEL) return prev;  // a frame without bones keeps the last shape
    return peOn == 1 ? TPS_HEEL : TPS_BOOT;
}
// the print's distance left / right of the walking line (the root between the feet), by shape (u)
static float TpShapeSide(int shape)
{
    return shape == TPS_BOOT ? 11.0f : shape == TPS_HEEL ? 8.0f : 10.0f;
}

// A print is one or two parts that never overlap (the draw is additive: an overlap would show as a bright seam):
// - a FAN: an egg fanned from its centre. Centre (ca, cb); half length la; half width wf at the front end, wr at the back
//   (blended along the length); the front half leans `tilt` toward the walking line (b -= tilt x max(0, cos th): the
//   big toe's side); the centre's heat kc, the rim's ke (x the print's own fade); ns rim points. v3.17.1's oval, the
//   heel's dot.
// - a SOLE: stations along the foot, heel to toe, each with its inner edge bi (< 0: toward the walking line), its outer
//   edge bo and its heat k (at the middle; the edges TP_EDGE_K of it): a strip of 4 triangles between two stations. The
//   pressure of a step: the heel and the ball warmest, the arch and the toe tip cooler; the inner edge cut in at the arch.
typedef struct { float ca, cb, la, wf, wr, tilt, kc, ke; int ns; } TpPad;
typedef struct { float a, bi, bo, k; } TpSt;
#define TP_ST_MAX 14
#define TP_EDGE_K 0.40f
enum { TPP_FAN, TPP_SOLE };
typedef struct { int kind; TpPad fan; int nst; TpSt st[TP_ST_MAX]; } TpPart;
#define TP_PARTS_MAX 2
typedef struct { int n; TpPart part[TP_PARTS_MAX]; } TpShapeDef;
// sizes at bind scale (world units: ~0.5 cm a unit; Henry's boot ~29 x 11 cm, Eileen's shoe ~23 x 7 cm; sizes.md)
static const TpShapeDef kTpShape[TPS_N] = {
    { 1, { { TPP_FAN, { 0.0f, 0.0f, 21.0f, 12.0f, 12.0f, 0.0f, 1.00f, 0.35f, 12 } } } },  // v3.17.1's oval (TP_L 42 x TP_W 24)
    { 1, { { TPP_SOLE, { 0 }, 13, {                  // Henry's boot: one sole, heel (a -28.5) to toe (+29)
        { -28.5f, -4.0f, 5.0f, 0.55f }, { -26.0f, -7.5f, 8.5f, 0.85f }, { -21.0f, -9.0f, 9.8f, 0.95f },   // the heel ...
        { -15.0f, -8.8f, 9.6f, 0.95f }, { -10.0f, -6.5f, 8.6f, 0.70f },
        { -5.0f, -3.0f, 8.0f, 0.55f }, { 0.0f, -4.5f, 9.5f, 0.65f },                                       // ... the arch (cut in) ...
        { 5.0f, -8.5f, 10.8f, 0.85f }, { 11.0f, -10.8f, 11.6f, 1.00f }, { 17.0f, -10.5f, 10.8f, 0.95f },  // ... the ball ...
        { 22.0f, -9.0f, 9.0f, 0.80f }, { 26.0f, -6.0f, 6.5f, 0.60f }, { 29.0f, -2.5f, 2.0f, 0.40f } } } } },  // ... the toe, leaning in
    { 2, { { TPP_SOLE, { 0 }, 8, {                   // Eileen's heeled shoe: the sole under the ball and the toes ...
        { -5.5f, -2.0f, 2.5f, 0.50f }, { -2.0f, -3.5f, 5.0f, 0.65f }, { 3.0f, -6.0f, 6.8f, 0.90f }, { 9.0f, -7.2f, 7.2f, 1.00f },
        { 15.0f, -7.0f, 5.8f, 0.90f }, { 19.5f, -6.0f, 3.5f, 0.70f }, { 22.5f, -4.5f, 0.5f, 0.50f }, { 23.5f, -3.2f, -1.0f, 0.35f } } },
           { TPP_FAN, { -19.0f, 0.0f, 4.2f, 4.2f, 4.2f, 0.0f, 1.00f, 0.45f, 8 } } } },        // ... and the heel's tip, 9 u behind it
};
#define TP_TRI_MAX 56  // triangles of one print (the boot: 12 x 4 = 48; Eileen's: 7 x 4 + 8 = 36; the oval 12)
typedef struct { float a, b, k; } TpPt;
// the rim point s of a fan (0 .. ns, ns = 0 again)
static void TpPadRim(const TpPad *p, int s, TpPt *o)
{
    const float th = 6.2831853f * (float)s / (float)p->ns, c = cosf(th), w = p->wr + (p->wf - p->wr) * 0.5f * (1.0f + c);
    o->a = p->ca + p->la * c;
    o->b = p->cb + w * sinf(th) - p->tilt * (c > 0.0f ? c : 0.0f);
    o->k = p->ke;
}
static void TpStPt(const TpSt *s, int col, TpPt *o)  // a station's inner edge (0), middle (1), outer edge (2)
{
    o->a = s->a;
    o->b = col == 0 ? s->bi : col == 2 ? s->bo : 0.5f * (s->bi + s->bo);
    o->k = col == 1 ? s->k : s->k * TP_EDGE_K;
}
static int TpTri(TpPt *t, int n, int max, const TpPt *a, const TpPt *b, const TpPt *c)
{
    if (n >= max) return n;
    t[3 * n] = *a, t[3 * n + 1] = *b, t[3 * n + 2] = *c;
    return n + 1;
}
// the triangles of a shape (3 points each, in the print's frame, k = the heat x 1 at a fresh print); returns the count
static int TpShapeTris(int shape, TpPt *t, int max)
{
    const TpShapeDef *d = &kTpShape[shape >= 0 && shape < TPS_N ? shape : TPS_OVAL];
    int i, s, n = 0;
    for (i = 0; i < d->n; ++i) {
        const TpPart *q = &d->part[i];
        if (q->kind == TPP_FAN) {
            const TpPad *p = &q->fan;
            for (s = 0; s < p->ns; ++s) {
                TpPt c, r0, r1;
                c.a = p->ca, c.b = p->cb, c.k = p->kc;
                TpPadRim(p, s, &r0);
                TpPadRim(p, s + 1, &r1);
                n = TpTri(t, n, max, &c, &r0, &r1);
            }
        } else {
            for (s = 0; s + 1 < q->nst; ++s) {
                TpPt p0[3], p1[3];
                int c;
                for (c = 0; c < 3; ++c) TpStPt(&q->st[s], c, &p0[c]), TpStPt(&q->st[s + 1], c, &p1[c]);
                n = TpTri(t, n, max, &p0[0], &p1[0], &p0[1]);  // (one turning, as the fans: no fold between the strips)
                n = TpTri(t, n, max, &p0[1], &p1[0], &p1[1]);
                n = TpTri(t, n, max, &p0[1], &p1[1], &p0[2]);
                n = TpTri(t, n, max, &p0[2], &p1[1], &p1[2]);
            }
        }
    }
    return n;
}
// the print's frame in the world: fwd (fx, fz) the walk direction (unit), side +1 / -1 (the print lies at side x out of the
// line, out = side x (-fz, fx)); a / b -> x / z
static void TpShapeToWorld(const TpPt *p, float cx, float cz, float fx, float fz, int side, float sc, float *x, float *z)
{
    const float ox = -fz * (float)side, oz = fx * (float)side;
    *x = cx + (p->a * fx + p->b * ox) * sc;
    *z = cz + (p->a * fz + p->b * oz) * sc;
}
#endif
