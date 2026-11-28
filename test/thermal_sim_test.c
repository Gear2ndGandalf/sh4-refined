// v3.59 TV359: thermal_sim.h native test (staging/v359_feedback/w3_thermal/PLAN.md): Thermal Vision's two pure parts.
// 1. Walls hide heat: when the light grid draws without a depth write (ThDepthWanted) and when this frame's depth is the
//    scene's (ThPrintDepth), every input.
// 2. The footprints: who is played (TpBodyShape: the hip bones' spacing, the dead band, the fallbacks), the three shapes
//    (TpShapeTris: sizes, Eileen's sole and heel apart, Henry's one sole, the old oval exactly as v3.17.1 drew it, the warm
//    centre and cool rim), and the mirror between the right and left prints (TpShapeToWorld).
// Build and run: test\build_thermal_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "../src/thermal_sim.h"

static int g_fail, g_n;
static void Check(int ok, const char *what)
{
    ++g_n;
    if (!ok) {
        printf("FAIL  %s\n", what);
        ++g_fail;
    }
}

static void TestDepth(void)
{
    int k, h, l, n = 0;
    for (k = 0; k < 2; ++k)
        for (h = 0; h < 2; ++h)
            for (l = 0; l < 2; ++l)
                n += ThDepthWanted(k, h, l);
    Check(n == 1 && ThDepthWanted(1, 1, 1), "the grid keeps the depth only with the key on, the hook in and Thermal Vision's heat live");
    Check(!ThPrintDepth(0, 0, 0) && !ThPrintDepth(0, 1, 1) && !ThPrintDepth(0, 0, 1), "no hook: the depth is not known (no depth test)");
    Check(ThPrintDepth(1, 0, 0), "hook in, no grid this frame (a room without such lights): the scene's own depth");
    Check(ThPrintDepth(1, 1, 1), "hook in, the grid drew keeping the depth");
    Check(!ThPrintDepth(1, 1, 0), "hook in, the grid drew with its depth write (Thermal Vision was off at the grid): overwritten");
    Check(TH_GRID_CALL_VA == 0x4EFBDCu && TH_GRID_FN_VA == 0x433F00u && TH_GRID_FLAG_VA == 0x137F978u, "the sites");
    {   // the call's rel32 (E8 1F 43 F4 FF) leads to 0x433F00
        const unsigned rel = 0xFFF4431Fu;
        Check(TH_GRID_CALL_VA + 5u + rel == TH_GRID_FN_VA, "E8 1F 43 F4 FF at 004EFBDC calls 00433F00");
    }
    printf("depth: the grid's no-z-write rule and the prints' depth checked\n");
}

static void TestBody(void)
{
    Check(TpBodyShape(TP_HIPS_E, 1, -1, -1) == TPS_HEEL, "Eileen's 24.1 u hips: heels");
    Check(TpBodyShape(TP_HIPS_H, 1, -1, -1) == TPS_BOOT, "Henry's 36.0 u hips: boots");
    Check(TpBodyShape(TP_HIPS_E, 1, 0, TPS_BOOT) == TPS_HEEL, "a live switch to Eileen (bones win over Playable Eileen's switch)");
    Check(TpBodyShape(TP_HIPS_H, 1, 1, TPS_HEEL) == TPS_BOOT, "a live switch to Henry (bones win)");
    Check(TpBodyShape(30.0f, 1, -1, TPS_BOOT) == TPS_BOOT && TpBodyShape(30.0f, 1, -1, TPS_HEEL) == TPS_HEEL, "the dead band keeps the shape");
    Check(TpBodyShape(29.5f, 1, -1, -1) == TPS_HEEL && TpBodyShape(30.5f, 1, -1, -1) == TPS_BOOT, "the dead band's first pick: the nearer body");
    Check(TpBodyShape(TP_HIPS_TO_E, 1, -1, TPS_BOOT) == TPS_HEEL && TpBodyShape(TP_HIPS_TO_H, 1, -1, TPS_HEEL) == TPS_BOOT, "the band's edges");
    Check(TpBodyShape(0.0f, 0, 1, -1) == TPS_HEEL && TpBodyShape(0.0f, 0, 0, -1) == TPS_BOOT && TpBodyShape(0.0f, 0, -1, -1) == TPS_BOOT,
          "no bones: Playable Eileen on = heels, off / not loaded = boots");
    Check(TpBodyShape(0.0f, 0, 0, TPS_HEEL) == TPS_HEEL && TpBodyShape(0.0f, 0, 1, TPS_BOOT) == TPS_BOOT, "no bones this time: the last shape stays");
    Check(TpBodyShape(500.0f, 1, 1, -1) == TPS_HEEL && TpBodyShape(1.0f, 1, 0, -1) == TPS_BOOT, "a reading out of bounds is no reading");
    {
        float x = 0.0f;
        volatile float zero = 0.0f;
        x = zero / zero;  // NaN
        Check(TpBodyShape(x, 1, 1, -1) == TPS_HEEL, "a NaN reading is no reading");
    }
    Check(TpShapeSide(TPS_BOOT) > TpShapeSide(TPS_HEEL) && TpShapeSide(TPS_OVAL) == 10.0f, "Henry's feet farther apart; the oval as v3.17.1 (TP_SIDE 10)");
    printf("body: Henry / Eileen by the hip bones checked\n");
}

typedef struct { float a0, a1, b0, b1, kmin, kmax; int n; } Box;
static Box ShapeBox(int shape, TpPt *t)
{
    Box b = { 1e9f, -1e9f, 1e9f, -1e9f, 1e9f, -1e9f, 0 };
    int i;
    b.n = TpShapeTris(shape, t, TP_TRI_MAX);
    for (i = 0; i < 3 * b.n; ++i) {
        if (t[i].a < b.a0) b.a0 = t[i].a;
        if (t[i].a > b.a1) b.a1 = t[i].a;
        if (t[i].b < b.b0) b.b0 = t[i].b;
        if (t[i].b > b.b1) b.b1 = t[i].b;
        if (t[i].k < b.kmin) b.kmin = t[i].k;
        if (t[i].k > b.kmax) b.kmax = t[i].k;
    }
    return b;
}
// every triangle of a print turns the same way in (a, b) (a strip or a fan without folds: no two triangles overlap)
static int Orient(const TpPt *t, int n)
{
    int i, pos = 0, neg = 0;
    for (i = 0; i < n; ++i) {
        const float cr = (t[3 * i + 1].a - t[3 * i].a) * (t[3 * i + 2].b - t[3 * i].b) - (t[3 * i + 1].b - t[3 * i].b) * (t[3 * i + 2].a - t[3 * i].a);
        if (cr > 1e-6f) ++pos;
        else if (cr < -1e-6f) ++neg;
    }
    return (pos == 0) != (neg == 0);
}
// is there a shape point with a in (lo, hi)? (a gap between parts has none)
static int AnyIn(const TpPt *t, int n, float lo, float hi)
{
    int i;
    for (i = 0; i < 3 * n; ++i)
        if (t[i].a > lo && t[i].a < hi) return 1;
    return 0;
}
static void TestShapes(void)
{
    TpPt t[3 * TP_TRI_MAX];
    Box o, h, e;
    int i;
    o = ShapeBox(TPS_OVAL, t);
    Check(o.n == 12, "the oval: 12 triangles (v3.17.1's fan)");
    Check(fabsf(o.a1 - o.a0 - 42.0f) < 0.01f && fabsf(o.b1 - o.b0 - 24.0f) < 0.2f, "the oval: 42 x 24 (TP_L x TP_W)");
    for (i = 0; i < 12; ++i) {  // v3.17.1: rim s at (21 cos, 12 sin), k 0.35; centre k 1
        const float th = 6.2831853f * (float)i / 12.0f;
        Check(fabsf(t[3 * i + 1].a - 21.0f * cosf(th)) < 1e-3f && fabsf(t[3 * i + 1].b - 12.0f * sinf(th)) < 1e-3f &&
              t[3 * i].k == 1.0f && t[3 * i + 1].k == 0.35f && t[3 * i].a == 0.0f && t[3 * i].b == 0.0f, "the oval point by point as v3.17.1");
    }
    h = ShapeBox(TPS_BOOT, t);
    Check(h.n == 48 && h.n <= TP_TRI_MAX, "Henry's boot: 48 triangles (12 strips)");
    Check(h.a1 - h.a0 > 55.0f && h.a1 - h.a0 < 60.0f, "Henry's boot: 55 .. 60 u long (a 29 cm boot)");
    Check(h.b1 - h.b0 > 20.0f && h.b1 - h.b0 < 26.0f, "Henry's boot: 20 .. 26 u wide");
    Check(kTpShape[TPS_BOOT].n == 1 && kTpShape[TPS_BOOT].part[0].kind == TPP_SOLE, "Henry's boot: one sole");
    {   // the waist: the outer edge carries the sole, the inner one (the arch) is cut in: left and right prints differ
        float in = 1e9f, out = -1e9f;
        for (i = 0; i < 3 * h.n; ++i)
            if (t[i].a > -6.0f && t[i].a < 1.0f) {
                if (t[i].b < in) in = t[i].b;
                if (t[i].b > out) out = t[i].b;
            }
        Check(in > -5.0f && out > 7.5f, "Henry's boot: the arch cut in on the inner edge (left and right prints differ)");
    }
    Check(h.kmax <= 1.0f && h.kmin >= 0.15f, "Henry's boot: heat 0.15 .. 1 of the print's");
    Check(Orient(t, h.n), "Henry's boot: every triangle turns the same way (no fold, no overlap: no bright seam)");
    e = ShapeBox(TPS_HEEL, t);
    Check(e.n == 36, "Eileen's shoe: 36 triangles (7 strips + an 8-fan)");
    Check(e.a1 - e.a0 > 44.0f && e.a1 - e.a0 < 50.0f, "Eileen's shoe: 44 .. 50 u long");
    Check(e.b1 - e.b0 > 13.0f && e.b1 - e.b0 < 18.0f && e.b1 - e.b0 < h.b1 - h.b0 - 4.0f, "Eileen's shoe: narrower than Henry's boot");
    Check(!AnyIn(t, e.n, -14.7f, -5.6f), "Eileen's shoe: the sole and the heel's dot apart (a 9 u gap: they cannot overlap)");
    Check(AnyIn(t, e.n, -23.3f, -14.8f), "Eileen's shoe: the heel's dot behind the sole");
    Check(Orient(t, e.n), "Eileen's shoe: every triangle turns the same way");
    {   // the soles: stations heel to toe, the inner edge left of the outer one; the toe leans in (the big toe's side)
        int sh, ok = 1;
        for (sh = TPS_BOOT; sh <= TPS_HEEL; ++sh) {
            const TpPart *q = &kTpShape[sh].part[0];
            Check(q->nst >= 2 && q->nst <= TP_ST_MAX, "a sole's station count");
            for (i = 0; i < q->nst; ++i) {
                if (i && !(q->st[i].a > q->st[i - 1].a)) ok = 0;
                if (!(q->st[i].bi < q->st[i].bo) || !(q->st[i].k > 0.0f && q->st[i].k <= 1.0f)) ok = 0;
            }
            Check(0.5f * (q->st[q->nst - 1].bi + q->st[q->nst - 1].bo) < 0.0f, "the toe tip leans toward the walking line");
        }
        Check(ok, "the soles' stations: heel to toe, inner edge < outer edge, heat in (0, 1]");
    }
    for (i = 0; i < 3 * e.n; ++i) Check(_finite(t[i].a + t[i].b + t[i].k) && t[i].k > 0.0f, "every point finite, every heat > 0");
    Check(TpShapeTris(TPS_BOOT, t, 5) == 5, "the triangle cap holds");
    Check(TpShapeTris(99, t, TP_TRI_MAX) == 12, "an unknown shape draws the oval");
    printf("shapes: oval %d, boot %d (%.1f x %.1f u), heels %d (%.1f x %.1f u) checked\n", o.n, h.n, h.a1 - h.a0, h.b1 - h.b0, e.n,
           e.a1 - e.a0, e.b1 - e.b0);
}

static void TestMirror(void)
{
    TpPt t[3 * TP_TRI_MAX];
    const int n = TpShapeTris(TPS_HEEL, t, TP_TRI_MAX);
    const float dirs[3][2] = { { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.6f, -0.8f } };
    int i, d, ok = 1, along = 1;
    for (d = 0; d < 3; ++d) {
        const float fx = dirs[d][0], fz = dirs[d][1];
        for (i = 0; i < 3 * n; ++i) {
            float xr, zr, xl, zl, ar, br, al, bl;
            TpShapeToWorld(&t[i], 0.0f, 0.0f, fx, fz, 1, 1.0f, &xr, &zr);
            TpShapeToWorld(&t[i], 0.0f, 0.0f, fx, fz, -1, 1.0f, &xl, &zl);
            ar = xr * fx + zr * fz, br = xr * -fz + zr * fx;  // along the walk / across it (+ = the +1 side)
            al = xl * fx + zl * fz, bl = xl * -fz + zl * fx;
            if (fabsf(ar - al) > 1e-3f || fabsf(br + bl) > 1e-3f) ok = 0;  // the mirror about the walking line
            if (fabsf(ar - t[i].a) > 1e-3f || fabsf(br - t[i].b) > 1e-3f) along = 0;  // + side: b is away from the line
        }
    }
    Check(ok, "the left print is the right one mirrored about the walking line (every walk direction)");
    Check(along, "a runs along the walk (the toe ahead), b away from the line on the +1 side");
    {   // the print's offset (TpStep: p + side x side_d x (-uz, ux)) is on the side b points to
        float x, z;
        const TpPt c = { 0.0f, 10.0f, 1.0f };
        const float px = 0.0f - 1.0f * 8.0f * -1.0f, pz = 0.0f + 0.0f * 8.0f * -1.0f;  // TpStep: walking +z, side -1, side_d 8
        TpShapeToWorld(&c, 0.0f, 0.0f, 0.0f, 1.0f, -1, 1.0f, &x, &z);
        Check(fabsf(x - 10.0f) < 1e-3f && fabsf(z) < 1e-3f && px > 0.0f && fabsf(pz) < 1e-3f,
              "b's side is the print's own side of the line (walking +z, side -1: both +x)");
    }
    {   // scale
        float x1, z1, x2, z2;
        TpShapeToWorld(&t[1], 5.0f, 7.0f, 1.0f, 0.0f, 1, 1.0f, &x1, &z1);
        TpShapeToWorld(&t[1], 5.0f, 7.0f, 1.0f, 0.0f, 1, 0.65f, &x2, &z2);
        Check(fabsf((x2 - 5.0f) - 0.65f * (x1 - 5.0f)) < 1e-3f && fabsf((z2 - 7.0f) - 0.65f * (z1 - 7.0f)) < 1e-3f, "the print's scale (the dog 0.65)");
    }
    printf("mirror: right / left prints checked\n");
}

// --dump <file>: every shape's triangles (shape a b k per point, three lines a triangle) for tools\footprint_preview.py
static int Dump(const char *path)
{
    TpPt t[3 * TP_TRI_MAX];
    FILE *f = fopen(path, "w");
    int sh, i, n;
    if (!f) return 1;
    for (sh = 0; sh < TPS_N; ++sh) {
        n = TpShapeTris(sh, t, TP_TRI_MAX);
        for (i = 0; i < 3 * n; ++i) fprintf(f, "%d %.4f %.4f %.4f\n", sh, t[i].a, t[i].b, t[i].k);
    }
    fprintf(f, "side %d %.2f %d %.2f %d %.2f\n", TPS_OVAL, TpShapeSide(TPS_OVAL), TPS_BOOT, TpShapeSide(TPS_BOOT), TPS_HEEL,
            TpShapeSide(TPS_HEEL));
    fclose(f);
    printf("dumped the shapes to %s\n", path);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--dump")) return Dump(argv[2]);
    TestDepth();
    TestBody();
    TestShapes();
    TestMirror();
    printf("%s: %d checks, %d failed\n", g_fail ? "FAILED" : "all passed", g_n, g_fail);
    return g_fail ? 1 : 0;
}
