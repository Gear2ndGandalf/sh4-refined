// ================================================================ v3.27.0 / v3.27.1 Death Presence: the sprites (PLAN 2-5)
// Included by deathpresence_draw.inc. v3.27.1 (staging/v3271_dp_art/PLAN.md; the look: renders/NOTES_v5.md, v5b Strong):
// the volume of smoke puffs, her black halo and violet tendrils, the victims by their stain, the Spirit Vision / Thermal /
// night-vision looks; all through Death Presence's own sprite list and ribbons (Spirit Vision's list is not used).
// Hashed lifetimes: no per-frame randomness; every motion in real seconds (g_dpNow).
static const float kDpVPuffCol[3] = { 0.075f, 0.020f, 0.130f };
static const float kDpMoteCol[3] = { 0.55f, 0.18f, 0.95f };
#define DP_WISP_N 48
typedef struct { float anchor[3]; int cycle; } DpWispSlot;
static DpWispSlot g_dpWisp[DP_WISP_N];
static int g_dpLook;  // this frame: 0 the full look, 1 Thermal (no additive light), 2 night vision (dark, no additive), 3 Spirit Vision
static float DpLerp(float a, float b, float t) { return a + (b - a) * t; }
// v3.27.2 (PLAN 5): the dark parts' colour per look: night vision pitch black, Thermal near-black (drawn after the palette)
static float DpLookC(void) { return g_dpLook == 2 ? 0.0f : g_dpLook == 1 ? 0.08f : 1.0f; }
// night vision filaments, drawn after the NV shader (review DP3272-001: their final colour): green-white in green night
// vision, near white in monochrome
static const float kDpNvGreen[3] = { 0.55f, 0.90f, 0.60f }, kDpNvMono[3] = { 0.80f, 0.84f, 0.80f };
#define kDpNvCol (g_fx[FX_FILTER] == FXF_NVMONO ? kDpNvMono : kDpNvGreen)
static int g_dpDiag;  // (v3.27.2: the probe draws its own untextured quads, outside the sprite list: no suppression)
static float g_dpZMul = 1.0f;  // the depth probe's scale (PLAN 2.4); 1 otherwise

// ---------------------------------------------------------------- v3.27.1 the DP sprite list and ribbons (PLAN 2.5, 3.3)
// Its own list (Spirit Vision's g_svSpr / SV_SPR_MAX and SvAuraDraw stay untouched): camera-facing quads projected as
// SvSprAdd does, with the DP near fade (20 -> 90 u, PLAN 2.2) and an optional size fade; a texture per entry (the SV
// atlas 4 x 4 or FXX_SMOKE 2 x 2). Drawn far to near: the premultiplied batch in runs of one texture, then the additive.
#define DP_SL_MAX    2048
#define DP_RIB_MAX   128      // ribbons a frame (her tendrils 2 x 18, the Spirit Vision veins 2 x 40)
#define DP_RIB_PTS   40       // points a ribbon
enum { DPT_ATLAS, DPT_SMOKE, DPT_FLAME };  // v3.28 TRAIL: DPT_FLAME = FXX_FLAME (64 x 128, one teardrop: the trail's tongues)
typedef struct { float x, y, z, rhw, rx, ry, cs, sn, a, depth, rgb[3], c4[4]; BYTE cell, add, tex, prio, fl; float top, sk, sdx, sdy; } DpSl;  // fl: c4 = corner heights
static DpSl g_dpSL[DP_SL_MAX];
static int g_dpSLN, g_dpSLDrop, g_dpSLDrawn;
static float g_dpNear0 = DP_NEAR0, g_dpNear1 = DP_NEAR1, g_dpFloorY, g_dpAlphaK = 1.0f;
static float g_dpRW[3], g_dpDW[3];  // this frame's camera right / screen-down as world unit vectors (DpBasis)
static int g_dpBasisOk;
// review DP3271-IR-002: a screen-aligned quad at view depth w is offset in the world along the camera's right and down
// vectors: the gradients of GvXform's h0 / h1 without their part along the w gradient (as SvSprScale), signed by a test
// projection so that +x / +y on the screen are +R / +D in the world
static void DpBasis(const Viewport8 *vp, const float at[3])
{
    const float c3[3] = { g_gvM[3], g_gvM[7], g_gvM[11] };
    const float l3 = sqrtf(c3[0] * c3[0] + c3[1] * c3[1] + c3[2] * c3[2]);
    float *out[2] = { g_dpRW, g_dpDW };
    int a, j;
    g_dpBasisOk = 0;
    if (!(l3 > 1e-6f) || !_finite(l3)) return;
    for (a = 0; a < 2; ++a) {
        float c[3], d = 0.0f, l, h0[4], h1[4], x0, y0, x1, y1, q[3];
        for (j = 0; j < 3; ++j) c[j] = g_gvM[4 * j + a];
        for (j = 0; j < 3; ++j) d += c[j] * c3[j] / l3;
        for (j = 0; j < 3; ++j) c[j] -= d * c3[j] / l3;
        l = sqrtf(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
        if (!(l > 1e-9f) || !_finite(l)) return;
        for (j = 0; j < 3; ++j) out[a][j] = c[j] / l, q[j] = at[j] + 10.0f * out[a][j];
        GvXform(at, h0);
        GvXform(q, h1);
        if (!GvScreen(h0, vp, &x0, &y0) || !GvScreen(h1, vp, &x1, &y1)) return;
        if ((a == 0 ? x1 - x0 : y1 - y0) < 0.0f)
            for (j = 0; j < 3; ++j) out[a][j] = -out[a][j];
    }
    g_dpBasisOk = 1;
}
#define DPF_CEIL  4   // v3.27.2 (review DP3272-006): also clipped at g_dpCeilH above the floor, fading 20 u below it
static float g_dpCeilH = 1.0e9f;  // the cloak's upper cut: her shoulder line (height above g_dpFloorY), set per frame
#define DPF_SIZE  1   // a puff larger than its camera distance dissolves (PLAN 2.2)
#define DPF_FLOOR 2   // the quad is clipped at the floor and faded over 0..35 u above it in its own plane (DpSlDraw): no hard cut
// the near fade of a world point (1 = far enough); also returns its eye distance
static float DpNearK(const float p[3], float *dOut)
{
    float d;
    if (g_svEyeOk) {
        const float dx = p[0] - g_svEye[0], dy = p[1] - g_svEye[1], dz = p[2] - g_svEye[2];
        d = sqrtf(dx * dx + dy * dy + dz * dz);
    } else {
        d = 1.0e4f;
    }
    if (dOut) *dOut = d;
    return FxSmooth(g_dpNear0, g_dpNear1, d);
}
// one sprite: world centre p, world radius wr, screen rotation, texture (DPT_*) and cell, batch (add 1 = additive),
// colour and strength (premultiplied: opacity; additive: brightness), a minimum screen radius, flags DPF_*, prio (0 = the
// first replaced when the list is full: far volume puffs), bias = world units the sort key is moved toward the camera
// (the halo over the fog right in front of it: NOTES_v5 3)
// v3.27.2 (v7 art, review DP3272-ART-001): a sprite stretched x g_dpStretchK along the screen direction g_dpStretchD (unit
// in the sprite's own rx / ry space): her hem streams behind her when she walks; 1 = round (every other sprite)
static float g_dpStretchK = 1.0f, g_dpStretchD[2] = { 1.0f, 0.0f };
static void DpSlCorner(const DpSl *s, float cx, float cy, float *ox, float *oy)  // a corner's pixel offset from the centre
{
    float ux = cx * s->cs - cy * s->sn, uy = cx * s->sn + cy * s->cs;
    if (s->sk > 1.0f) {
        const float d = (ux * s->sdx + uy * s->sdy) * (s->sk - 1.0f);
        ux += d * s->sdx, uy += d * s->sdy;
    }
    *ox = ux * s->rx, *oy = uy * s->ry;
}
static void DpSlAdd(const float p[3], float wr, float rot, int tex, int cell, int add, const float rgb[3], float a, float minPx,
                    int flags, int prio, float bias)
{
    const Viewport8 *vp = g_svVp;
    float h[4], x, y, rx, ry, m, d;
    DpSl *s;
    int c;
    a *= g_dpAlphaK;  // (a victim's end-of-stain fade: review DP3271-001)
    if (!(a > 0.004f) || !(wr > 0.0f) || !vp) return;
    if (add && g_dpLook == 1 && !g_dpDiag) return;  // Thermal: no additive light (v3.27.2: night vision draws its filaments, PLAN 5.3)
    a *= DpNearK(p, &d);
    if (flags & DPF_SIZE) a *= FxSmooth(0.4f, 1.0f, d / wr);
    if (!(a > 0.004f)) return;
    GvXform(p, h);
    if (!GvScreen(h, vp, &x, &y)) return;  // also the w >= GV_NEAR_W near clip
    rx = wr * g_svPxX / h[3];
    ry = wr * g_svPxY / h[3];
    m = rx > ry ? rx : ry;
    if (!_finite(m) || !(m > 0.0f)) return;
    if (m > SV_SPR_MAXH * (float)vp->h) {
        const float f = SV_SPR_MAXH * (float)vp->h / m;
        rx *= f, ry *= f;
    } else if (m < minPx) {
        const float f = minPx / m;
        rx *= f, ry *= f;
    }
    m = (rx > ry ? rx : ry) * (g_dpStretchK > 1.0f ? g_dpStretchK : 1.0f);
    if (x + 1.415f * m < (float)vp->x || x - 1.415f * m > (float)(vp->x + vp->w) || y + 1.415f * m < (float)vp->y ||
        y - 1.415f * m > (float)(vp->y + vp->h))
        return;
    if (g_dpSLN >= DP_SL_MAX) {  // full: replace a lower-priority entry (the first found), else drop this one
        int i;
        ++g_dpSLDrop;
        for (i = 0; i < DP_SL_MAX && g_dpSL[i].prio >= prio; ++i) {}
        if (i >= DP_SL_MAX) return;
        s = &g_dpSL[i];
    } else {
        s = &g_dpSL[g_dpSLN++];
    }
    if ((flags & DPF_FLOOR) && !(flags & DPF_CEIL)) {  // v3.27.3 (re_depth.md 5.3): a floor-touching puff (the volume,
        // the pool's puffs) takes the depth of a point half its radius nearer, so the floor in front of its centre does not
        // cut its lower half off ("half underground"); the cloak (DPF_CEIL) keeps its own depth (it must stay behind her)
        float hz[4];
        memcpy(hz, h, sizeof hz);
        hz[3] = h[3] - 0.5f * wr > GV_NEAR_W ? h[3] - 0.5f * wr : GV_NEAR_W;
        s->z = SvZH(hz) * g_dpZMul;
    } else {
        s->z = SvZH(h) * g_dpZMul;
    }
    s->x = x, s->y = y, s->rhw = 1.0f / h[3], s->depth = h[3] - bias;
    s->rx = rx, s->ry = ry, s->cs = cosf(rot), s->sn = sinf(rot);
    memcpy(s->rgb, rgb, sizeof s->rgb);
    s->a = a;
    s->cell = (BYTE)cell, s->add = (BYTE)(add != 0), s->tex = (BYTE)tex, s->prio = (BYTE)prio;
    for (c = 0; c < 4; ++c) s->c4[c] = 1.0f;
    s->fl = 0;
    s->top = (flags & DPF_CEIL) ? g_dpCeilH : 1.0e9f;
    s->sk = g_dpStretchK > 1.0f ? g_dpStretchK : 1.0f, s->sdx = g_dpStretchD[0], s->sdy = g_dpStretchD[1];
    if ((flags & (DPF_FLOOR | DPF_CEIL)) && g_dpBasisOk) {
        // review DP3271-IR-002: each displayed corner (rotated, after the screen-size clamp) back in the world: its pixel
        // offset / (pixels per world unit at w = 1) x w along the camera's right / down vectors; its height above the floor
        for (c = 0; c < 4; ++c) {
            const float cx = (c & 1) ? 1.0f : -1.0f, cy = (c & 2) ? 1.0f : -1.0f;
            float ox, oy, wx, wy;
            DpSlCorner(s, cx, cy, &ox, &oy);
            wx = ox / g_svPxX * h[3], wy = oy / g_svPxY * h[3];
            s->c4[c] = g_dpFloorY - (p[1] + wx * g_dpRW[1] + wy * g_dpDW[1]);  // the corner's height above the floor (Y down); linear
        }
        s->fl = 1;
    }
}
static DWORD DpColScale(DWORD c, float k, int add)  // a vertex colour x k (premultiplied: all four channels)
{
    const DWORD r = (DWORD)((float)(c >> 16 & 255) * k + 0.5f), g = (DWORD)((float)(c >> 8 & 255) * k + 0.5f),
                b = (DWORD)((float)(c & 255) * k + 0.5f), al = add ? 255u : (DWORD)((float)(c >> 24) * k + 0.5f);
    return al << 24 | r << 16 | g << 8 | b;
}
static int DpSlCmp(const void *a, const void *b)  // additive last; far first within a batch (premultiplied over)
{
    const DpSl *p = (const DpSl *)a, *q = (const DpSl *)b;
    if (p->add != q->add) return p->add < q->add ? -1 : 1;
    return p->depth < q->depth ? 1 : p->depth > q->depth ? -1 : 0;
}
static void DpSlStates(void *dev, void *tex, int add)
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    FxSampler(dev, tex, 0, 0);                    // clamp, linear
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 1 /*COLOROP*/, 1 /*DISABLE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, add ? 2 /*ONE*/ : 6 /*INVSRCALPHA*/);
}
// draws and empties the list; returns the quads drawn
// a vertex with its height above the floor; Sutherland-Hodgman against h >= thr (above 1) or h <= thr (above 0)
typedef struct { SpVtx v; float h; } DpHv;
static int DpClipH(const DpHv *in, int n, DpHv *out, float thr, int above)
{
    int k, no = 0;
    for (k = 0; k < n; ++k) {
        const DpHv *a = &in[k], *b = &in[(k + 1) % n];
        const int ia = above ? a->h >= thr : a->h <= thr, ib = above ? b->h >= thr : b->h <= thr;
        if (ia) out[no++] = *a;
        if (ia != ib) {
            const float t = (thr - a->h) / (b->h - a->h);
            DpHv *o = &out[no++];
            *o = *a;
            o->v.x = a->v.x + (b->v.x - a->v.x) * t, o->v.y = a->v.y + (b->v.y - a->v.y) * t;
            o->v.u = a->v.u + (b->v.u - a->v.u) * t, o->v.v = a->v.v + (b->v.v - a->v.v) * t;
            o->h = thr;
        }
    }
    return no >= 3 ? no : 0;
}
static int DpSlDraw(void *dev)
{
    static SpVtx v[6 * 256];
    void *tex[3];
    int i, n = 0, drawn = 0, runTex = -1, runAdd = -1;
    if (!g_dpSLN) return 0;
    tex[DPT_ATLAS] = FxTexture(dev, FXX_SVATLAS);
    tex[DPT_SMOKE] = FxTexture(dev, FXX_SMOKE);
    tex[DPT_FLAME] = FxTexture(dev, FXX_FLAME);  // v3.28 TRAIL (NULL: its runs are skipped)
    qsort(g_dpSL, (size_t)g_dpSLN, sizeof g_dpSL[0], DpSlCmp);
    for (i = 0; i <= g_dpSLN; ++i) {
        const DpSl *s = i < g_dpSLN ? &g_dpSL[i] : NULL;
        if (!s || s->tex != runTex || s->add != runAdd || n + 48 > (int)(sizeof v / sizeof v[0])) {  // flush the run
            if (n && runTex >= 0 && tex[runTex]) {
                ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
                drawn += n / 6;
            }
            n = 0;
            if (!s) break;
            if (s->tex != runTex || s->add != runAdd) {
                runTex = s->tex, runAdd = s->add;
                if (tex[runTex]) DpSlStates(dev, tex[runTex], runAdd);
            }
        }
        {
            // the SV atlas: 4 x 4 cells of a 256 texture; FXX_SMOKE: 4 x 2 cells of 256 in 1024 x 512 (half-texel insets)
            // v3.28: FXX_FLAME = the spirit fire atlas (spiritfire_sim.h): 8 x 4 cells of 64 x 128 in 512 x 512; the trail's
            // tongues use its flipbook frames 0..27 (merge of the TRAIL and FIRE rounds: the trail was built on the old single
            // 64 x 128 teardrop)
            const int fl = s->tex == DPT_FLAME;
            const float gu = fl ? 0.125f : 0.25f, gv = s->tex == DPT_SMOKE ? 0.5f : 0.25f,
                        iu = s->tex == DPT_SMOKE ? 1024.0f : fl ? 512.0f : 256.0f,
                        iv = s->tex == DPT_SMOKE ? 512.0f : fl ? 512.0f : 256.0f;
            const float u0 = gu * (float)(fl ? s->cell & 7 : s->cell & 3) + 0.5f / iu, v0 = gv * (float)(fl ? s->cell >> 3 : s->cell >> 2) + 0.5f / iv;
            const float u1 = u0 + gu - 1.0f / iu, v1 = v0 + gv - 1.0f / iv;
            SpVtx q[4];
            DWORD col;
            int m;
            if (s->add) {
                const float k = s->a * g_svMul;
                col = 0xFF000000u | (DWORD)(FxSat(s->rgb[0] * k) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(s->rgb[1] * k) * 255.0f + 0.5f) << 8 |
                      (DWORD)(FxSat(s->rgb[2] * k) * 255.0f + 0.5f);
            } else {
                const float a = FxSat(s->a * g_svMul);
                col = (DWORD)(a * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(s->rgb[0]) * a * 255.0f + 0.5f) << 16 |
                      (DWORD)(FxSat(s->rgb[1]) * a * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(s->rgb[2]) * a * 255.0f + 0.5f);
            }
            for (m = 0; m < 4; ++m) {
                const float cx = (m & 1) ? 1.0f : -1.0f, cy = (m & 2) ? 1.0f : -1.0f;
                float ox, oy;
                DpSlCorner(s, cx, cy, &ox, &oy);
                q[m].x = s->x + ox - 0.5f;
                q[m].y = s->y + oy - 0.5f;
                q[m].z = s->z;
                q[m].rhw = s->rhw;
                q[m].c = col;
                q[m].u = (m & 1) ? u1 : u0;
                q[m].v = (m & 2) ? v1 : v0;
            }
            if (!s->fl) {
                v[n++] = q[0], v[n++] = q[1], v[n++] = q[2];
                v[n++] = q[1], v[n++] = q[3], v[n++] = q[2];
            } else {  // review DP3271-R7-001 / DP3272-006: clip the quad (its height is linear across it) to [0, top] and
                      // split it at 35 u and at top - 20: alpha = min(h / 35, (top - h) / 20), exactly 0 at the floor / the top
                static const int kLoop[4] = { 0, 1, 3, 2 };
                DpHv P[4], A[6], Bx[8], G[3][10];
                int na, nb, ng[3], e;
                const float top = s->top, cut1 = 35.0f < top - 20.0f ? 35.0f : 0.5f * top, cut2 = top - 20.0f > cut1 ? top - 20.0f : cut1;
                for (m = 0; m < 4; ++m) P[m].v = q[kLoop[m]], P[m].h = s->c4[kLoop[m]];
                na = DpClipH(P, 4, A, 0.0f, 1);
                nb = top < 1.0e8f ? DpClipH(A, na, Bx, top, 0) : 0;
                if (top >= 1.0e8f) memcpy(Bx, A, sizeof A), nb = na;
                ng[0] = DpClipH(Bx, nb, G[0], cut1, 0);  // 0 .. cut1
                {
                    DpHv T[10];
                    const int nt = DpClipH(Bx, nb, T, cut1, 1);
                    ng[1] = DpClipH(T, nt, G[1], cut2, 0);  // cut1 .. cut2
                    ng[2] = DpClipH(T, nt, G[2], cut2, 1);  // cut2 .. top
                }
                for (e = 0; e < 3; ++e) {
                    int j;
                    for (j = 1; j + 1 < ng[e] && n + 3 <= (int)(sizeof v / sizeof v[0]); ++j) {
                        const DpHv *tri[3] = { &G[e][0], &G[e][j], &G[e][j + 1] };
                        int t3;
                        for (t3 = 0; t3 < 3; ++t3) {
                            const float hh = tri[t3]->h, k1 = hh / 35.0f, k2 = top < 1.0e8f ? (top - hh) / 20.0f : 1.0f;
                            v[n] = tri[t3]->v;
                            v[n].c = DpColScale(col, FxSat(k1 < k2 ? k1 : k2), s->add);
                            ++n;
                        }
                    }
                }
            }
        }
    }
    g_dpSLDrawn += drawn;
    g_dpSLN = 0;
    return drawn;
}
// ribbons: additive camera-facing strips along world points (the SvvTube way): per point a half width (world) and a
// brightness; a point behind the near plane or of zero brightness at both neighbours breaks the strip. FXX_SMOKE cell 6
// (DPS_STRAND: u across, v along). Built into one vertex array, drawn after the sprite list.
static SpVtx g_dpRibV[DP_RIB_MAX * (DP_RIB_PTS - 1) * 6];
static int g_dpRibN, g_dpRibs, g_dpRibDrop;  // (g_dpRibDrop: ribbons over DP_RIB_MAX this room, review DP3272-R3-001)
static void DpRibbon(const float (*P)[3], const float *w, const float *k, int n, const float rgb[3], float minPx)
{
    const Viewport8 *vp = g_svVp;
    float X[DP_RIB_PTS], Y[DP_RIB_PTS], Z[DP_RIB_PTS], RHW[DP_RIB_PTS], HW[DP_RIB_PTS], K[DP_RIB_PTS];
    int ok[DP_RIB_PTS], i;
    if (!vp || n < 2 || g_dpLook == 1) return;  // (night vision: green-white filaments)
    if (g_dpRibs >= DP_RIB_MAX) { ++g_dpRibDrop; return; }
    if (n > DP_RIB_PTS) n = DP_RIB_PTS;
    ++g_dpRibs;
    for (i = 0; i < n; ++i) {
        float h[4];
        GvXform(P[i], h);
        ok[i] = GvScreen(h, vp, &X[i], &Y[i]);
        if (!ok[i]) continue;
        Z[i] = SvZH(h), RHW[i] = 1.0f / h[3];
        HW[i] = w[i] * g_svPxX / h[3];
        if (HW[i] < minPx) HW[i] = minPx;
        K[i] = k[i] * DpNearK(P[i], NULL) * g_svMul;
    }
    for (i = 0; i + 1 < n; ++i) {
        float nx[2], ny[2];
        int e;
        if (!ok[i] || !ok[i + 1] || (K[i] < 0.004f && K[i + 1] < 0.004f)) continue;
        if (g_dpRibN + 6 > (int)(sizeof g_dpRibV / sizeof g_dpRibV[0])) return;
        for (e = 0; e < 2; ++e) {  // the perpendicular at each end, from its neighbours on screen
            const int j = i + e, a = j > 0 && ok[j - 1] ? j - 1 : j, b = j + 1 < n && ok[j + 1] ? j + 1 : j;
            float dx = X[b] - X[a], dy = Y[b] - Y[a], l = sqrtf(dx * dx + dy * dy);
            if (!(l > 1e-3f)) dx = 1.0f, dy = 0.0f, l = 1.0f;
            nx[e] = -dy / l * HW[j], ny[e] = dx / l * HW[j];
        }
        {
            SpVtx q[4];
            int m;
            for (m = 0; m < 4; ++m) {
                const int e = m >> 1, j = i + e, side = (m & 1) ? 1 : -1;
                const float kk = FxSat(K[j]);
                q[m].x = X[j] + side * nx[e] - 0.5f;
                q[m].y = Y[j] + side * ny[e] - 0.5f;
                q[m].z = Z[j];
                q[m].rhw = RHW[j];
                q[m].c = 0xFF000000u | (DWORD)(FxSat(rgb[0] * kk) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(rgb[1] * kk) * 255.0f + 0.5f) << 8 |
                         (DWORD)(FxSat(rgb[2] * kk) * 255.0f + 0.5f);
                q[m].u = (m & 1) ? 0.75f - 0.5f / 1024.0f : 0.5f + 0.5f / 1024.0f;  // cell 6: u 0.5..0.75, v 0.5..1
                q[m].v = 0.5f + 0.5f / 512.0f + (0.5f - 1.0f / 512.0f) * (float)j / (float)(n - 1);
            }
            g_dpRibV[g_dpRibN++] = q[0], g_dpRibV[g_dpRibN++] = q[1], g_dpRibV[g_dpRibN++] = q[2];
            g_dpRibV[g_dpRibN++] = q[1], g_dpRibV[g_dpRibN++] = q[3], g_dpRibV[g_dpRibN++] = q[2];
        }
    }
}
static int DpRibDraw(void *dev)
{
    void *tex;
    const int n = g_dpRibN;
    g_dpRibN = g_dpRibs = 0;
    if (!n || (tex = FxTexture(dev, FXX_SMOKE)) == NULL) return 0;
    DpSlStates(dev, tex, 1);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), g_dpRibV, sizeof(SpVtx));
    return n / 3;
}


// ---------------------------------------------------------------- v3.27.1 the look per level (renders/NOTES_v5.md, v5b Strong)
typedef struct {
    int N;                       // volume puffs
    float rad0, rad1, H, minY, dens, thin, dense, churn, bob, rmax;  // puff radius, dome height, min centre (x radius), ...
    float top0, top1, topK;      // v5b: puffs whose top is high thin out (Strong: the wall above ~150 u stays readable)
    float pool4, pool30;         // the floor pool layers' density
    int wisps, tN;               // raised wisps, her tendrils
    float tH0, tH1, tA, haloA, fringeA;
} DpLook;
static const DpLook kDpLook[DP_LEVELS] = {
    { 0 },
    { 66, 90.0f, 160.0f, 50.0f, 0.42f, 1.15f, 0.35f, 0.04f, 0.7f, 7.0f, 0.97f, 0.0f, 0.0f, 0.0f, 0.55f, 0.18f, 24, 7, 60.0f, 90.0f,
      0.70f, 0.65f, 0.20f },  // v3.27.2 v7: halo 0.65 (was 0.55)
    { 80, 70.0f, 130.0f, 125.0f, 0.45f, 0.90f, 0.30f, 0.05f, 1.8f, 16.0f, 0.70f, 100.0f, 230.0f, 0.70f, 0.55f, 0.18f, 48, 18, 60.0f,
      100.0f, 1.00f, 0.92f, 0.30f },  // v3.27.2 v7: halo 0.92 (was 0.80)
};
#define DP_VOL_MAX   96
#define DP_VOL_ALPHA 0.62f       // a puff's base peak alpha (x its hashed density, env, falloffs)
#define DP_VOL_GLOW  0.35f       // the violet crest light inside each puff (additive, x kDpHiCol)
#define DP_VOL_BRIGHT 1.5f       // the tint x this (premultiplied colour); the texture's billow shading x 1.55 (DpSmokeTexel)
enum { DPS_SMOKE0, DPS_SMOKE1, DPS_CREST0, DPS_CREST1, DPS_HALO, DPS_SOFT, DPS_STRAND };  // FXX_SMOKE cells (4 x 2)
typedef struct { float anchor[3]; int cycle; } DpVolSlot;
static DpVolSlot g_dpVol[DP_VOL_MAX];
static void DpVolReset(void)
{
    int i;
    for (i = 0; i < DP_VOL_MAX; ++i) g_dpVol[i].cycle = -1;
}
static void DpTintAt(float x, float z, float c[3])  // the v3.27.0 body tint (blue-violet / violet / magenta) at world XZ
{
    static const float kBlue[3] = { 0.010f, 0.014f, 0.062f }, kMag[3] = { 0.052f, 0.008f, 0.046f };
    const float t = g_dpNow, tn = FxSmooth(0.25f, 0.75f, DpFogN(x / 900.0f + 0.37f + 0.004f * t, z / 900.0f + 0.11f - 0.0028f * t));
    int k;
    for (k = 0; k < 3; ++k)
        c[k] = tn < 0.5f ? kDpBody[k] + (kBlue[k] - kDpBody[k]) * (1.0f - 2.0f * tn) : kDpBody[k] + (kMag[k] - kDpBody[k]) * (2.0f * tn - 1.0f);
}
static float DpSegDist(const float p[3], const float a[3], const float b[3])  // distance from p to the segment a-b
{
    float ab[3], ap[3], t, l2 = 0.0f, d2 = 0.0f;
    int k;
    for (k = 0; k < 3; ++k) ab[k] = b[k] - a[k], ap[k] = p[k] - a[k], l2 += ab[k] * ab[k];
    t = l2 > 1e-6f ? (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / l2 : 0.0f;
    t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
    for (k = 0; k < 3; ++k) {
        const float e = ap[k] - ab[k] * t;
        d2 += e * e;
    }
    return sqrtf(d2);
}

// ---------------------------------------------------------------- her bones (PLAN 3.1)
// PBONES of the preview, in the shared henry01 / Eileen hierarchy (KB section 3): head 11, neck 10, chest 9 (upper spine),
// pelvis 8, shoulders 18 / 13, elbows 19 / 14, wrists 20 / 15, hips 5 / 2, knees 6 / 3, feet 7 / 4
enum { HB_HEAD, HB_NECK, HB_CHEST, HB_PELVIS, HB_LSH, HB_RSH, HB_LELB, HB_RELB, HB_LHAND, HB_RHAND, HB_LHIP, HB_RHIP, HB_LKNEE,
       HB_RKNEE, HB_LFOOT, HB_RFOOT, HB_N };
static const BYTE kDpHerBone[HB_N] = { 11, 10, 9, 8, 18, 13, 19, 14, 20, 15, 5, 2, 6, 3, 7, 4 };
static const float kDpHaloR[HB_N] = { 26, 22, 34, 32, 24, 24, 22, 22, 22, 22, 26, 26, 24, 24, 22, 22 };
static const BYTE kDpHerEdge[15][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 1, 4 }, { 1, 5 }, { 4, 6 }, { 6, 8 }, { 5, 7 }, { 7, 9 },
                                        { 3, 10 }, { 3, 11 }, { 10, 12 }, { 12, 14 }, { 11, 13 }, { 13, 15 } };
static const float kDpHerEdgeR[15] = { 11, 13, 16, 7, 7, 5.5f, 4.5f, 5.5f, 4.5f, 9, 9, 8.5f, 6.5f, 8.5f, 6.5f };
static const BYTE kDpHerAxis[13][2] = { { 3, 2 }, { 2, 1 }, { 4, 6 }, { 6, 8 }, { 5, 7 }, { 7, 9 }, { 10, 12 }, { 12, 14 }, { 11, 13 },
                                        { 13, 15 }, { 1, 0 }, { 1, 4 }, { 1, 5 } };
static int g_dpHerOk, g_dpHerSeenRoom;
static float g_dpHer[HB_N][3];
static int DpHerBones(const float pp[3])
{
    const BYTE *m, *skel;
    const float *mats;
    int i;
    g_dpHerOk = 0;
    if (g_dpHerFrame != g_fxFrame) return 0;
    m = *(const BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    if (!m || (*(const BYTE *)(m + 0xE4) & 4)) return 0;  // first person: her body is hidden
    mats = *(const float *const *)(m + 0xC0);
    skel = *(const BYTE *const *)(m + 0xE0);
    if (!mats || !skel || *(const int *)(skel + 0x14) < 25) return 0;
    for (i = 0; i < HB_N; ++i) {
        const float *b = mats + 16 * kDpHerBone[i] + 12;
        if (!_finite(b[0] + b[1] + b[2]) || fabsf(b[0] - pp[0]) > 400.0f || fabsf(b[1] - pp[1]) > 400.0f || fabsf(b[2] - pp[2]) > 400.0f)
            return 0;
        memcpy(g_dpHer[i], b, 12);
    }
    return g_dpHerOk = 1;
}
// p moved `push` units away from the camera along the eye ray (the halo trick: her nearer body hides its middle)
static void DpAway(const float p[3], float push, float o[3])
{
    float e[3], l;
    int k;
    for (k = 0; k < 3; ++k) e[k] = p[k] - g_svEye[k];
    l = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    for (k = 0; k < 3; ++k) o[k] = p[k] + (l > 1e-3f ? e[k] / l * push : 0.0f);
}

// ---------------------------------------------------------------- A. the volume (PLAN 2.3; NOTES_v5 1, v5b)
static float g_dpGovK = 1.0f;  // v3.35 FX335: the effect budget's share of the volume's puffs (budget_sim.h BgDpKeep), faded
static LONG g_dpGovStamp = -1;
static void DpVolume(float R, int level, const float pp[3])
{
    const DpLook *L = &kDpLook[level];
    const float t = g_dpNow;
    float aim[3][3];
    int i, k;
    for (k = 0; k < 3; ++k) {  // the clear tunnel's aim points: her head, pelvis, feet
        static const float kH[3] = { 150.0f, 92.0f, 12.0f };
        memcpy(aim[k], pp, sizeof aim[k]);
        aim[k][1] -= kH[k];
    }
    if (g_dpHerOk) memcpy(aim[0], g_dpHer[HB_HEAD], 12), memcpy(aim[1], g_dpHer[HB_PELVIS], 12);
    for (i = 0; i < L->N && i < DP_VOL_MAX; ++i) {
        const float life = DpLerp(9.0f, 16.0f, FxRnd((DWORD)i * 8191u + 7u)) / L->churn;
        const float tt = t + FxRnd((DWORD)i * 92821u + 13u) * life;
        const int n = (int)floorf(tt / life);
        const float age = tt - (float)n * life, f = age / life;
        const DWORD kk = (DWORD)i * 271u + (DWORD)n * 1237u;
        const float rad = DpLerp(L->rad0, L->rad1, FxRnd(kk + 31u)), r0 = R * L->rmax * sqrtf(FxRnd(kk + 32u));
        const float om = DpLerp(0.04f, 0.10f, FxRnd(kk + 35u)) * L->churn * ((i & 1) ? 1.0f : -1.0f);
        const float th = 6.2831853f * FxRnd(kk + 34u) + om * age, ph = 6.28f * FxRnd(kk + 36u);
        const float rr = r0 * (1.0f + 0.05f * L->churn * sinf(0.3f * L->churn * t + ph));
        const float q = 1.0f - (r0 / R) * (r0 / R), hmax = L->H * sqrtf(q > 0.0f ? q : 0.0f);
        const float env = FxSmooth(0.0f, 0.2f, f) * (1.0f - FxSmooth(0.7f, 1.0f, f)), h = FxRnd(kk + 41u);
        const float size = rad * (0.9f + 0.2f * f);
        float y, d, dens, al, p[3], col[3], tun, hd;
        if (g_dpVol[i].cycle != n) {  // a new life: anchored where the fog's centre is now
            g_dpVol[i].cycle = n;
            memcpy(g_dpVol[i].anchor, g_dpCen, sizeof g_dpVol[i].anchor);
        } else {  // review DP3271-I01: the anchor chases the fog's centre (1.2 s): she walks through the cloud, which follows
            const float c = 1.0f - expf(-g_dpDt / 1.2f);  // and re-forms round her within a few seconds at any speed
            for (k = 0; k < 3; ++k) g_dpVol[i].anchor[k] += (g_dpCen[k] - g_dpVol[i].anchor[k]) * c;
        }
        y = L->minY * rad > hmax * powf(FxRnd(kk + 33u), 1.2f) ? L->minY * rad : hmax * powf(FxRnd(kk + 33u), 1.2f);
        y += L->bob * sinf(0.5f * L->churn * t + ph);
        if (y < L->minY * rad) y = L->minY * rad;
        d = h < L->thin ? DpLerp(0.10f, 0.38f, FxRnd(kk + 42u)) : h > 1.0f - L->dense ? DpLerp(1.5f, 2.0f, FxRnd(kk + 42u))
                                                                   : DpLerp(0.70f, 1.10f, FxRnd(kk + 42u));
        dens = (1.0f - FxSmooth(0.25f, 1.0f, rr / R)) * (1.0f - 0.45f * FxSmooth(40.0f, 190.0f, y));
        if (L->topK > 0.0f) dens *= 1.0f - L->topK * FxSmooth(L->top0, L->top1, y + 0.6f * size);
        al = DP_VOL_ALPHA * L->dens * env * dens * d;
        if (g_dpGovK < 0.999f) {  // v3.35 FX335: the budget thins the puffs (the last kept ones fade one after another, BgDpPuffK);
            const float gk = BgDpPuffK(i, L->N, g_dpGovK);  // the kept ones x (1 + 0.5 (1 - k)): the cloud keeps its body
            if (gk <= 0.0f) continue;
            al *= gk * (1.0f + 0.5f * (1.0f - g_dpGovK));
        }
        p[0] = g_dpVol[i].anchor[0] + rr * cosf(th);
        p[1] = g_dpVol[i].anchor[1] - y;  // Y down
        p[2] = g_dpVol[i].anchor[2] + rr * sinf(th);
        {   // review DP3271-I01: a puff left outside the fog (1.0 -> 1.25 R from its centre) fades. Review DP3271-I02 / R5-001:
            // walls, conservatively: the nearest real (unclamped) wall hit in its sector and both neighbours; the puff's disc
            // (the texture is 0 beyond its radius `size`) is gone by the time its far edge r + size reaches that hit:
            // alpha x (1 - smoothstep(hit - 40, hit, r + size))
            const float dx = p[0] - g_dpCen[0], dz = p[2] - g_dpCen[2], r = sqrtf(dx * dx + dz * dz);
            const int sec = ((int)floorf(atan2f(dz, dx) / 6.2831853f * (float)DP_WALLS + 0.5f) % DP_WALLS + DP_WALLS) % DP_WALLS;
            float hit = g_dpWallRaw[sec];
            const float h1 = g_dpWallRaw[(sec + 1) % DP_WALLS], h2 = g_dpWallRaw[(sec + DP_WALLS - 1) % DP_WALLS];
            hit = h1 < hit ? h1 : hit;
            hit = h2 < hit ? h2 : hit;
            al *= 1.0f - FxSmooth(1.0f * R, 1.25f * R, r);
            al *= 1.0f - FxSmooth(hit - DP_WALL_PRE, hit, r + size);
        }
        if (g_dpLook == 3) {  // Spirit Vision: additive spirit light, no tunnel, no tint
            static const float kSv[3] = { 0.30f, 0.30f, 0.95f };
            DpSlAdd(p, size, FxRnd(kk + 37u) * 6.28f, DPT_SMOKE, DPS_SMOKE0 + (int)(kk & 1u), 1, kSv,
                    3.2f * 0.035f * 0.6f * al / DP_VOL_ALPHA, 0.0f, DPF_SIZE | DPF_FLOOR, 0, 0.0f);  // v3.27.3: x 3.2 (v3.27.2 x 2.5)
            continue;
        }
        tun = 9.0e9f;  // the clear tunnel: the puffs on the eye rays to her head / pelvis / feet thin out
        if (g_svEyeOk)
            for (k = 0; k < 3; ++k) {
                const float e = DpSegDist(p, g_svEye, aim[k]);
                tun = e < tun ? e : tun;
            }
        al *= FxSmooth(0.10f * size + 15.0f, 0.60f * size + 45.0f, tun);
        hd = sqrtf((p[0] - pp[0]) * (p[0] - pp[0]) + (p[2] - pp[2]) * (p[2] - pp[2]));
        al *= 0.45f + 0.55f * FxSmooth(40.0f, 170.0f, hd);  // the lighter core round her legs
        DpTintAt(p[0], p[2], col);
        for (k = 0; k < 3; ++k) col[k] *= DP_VOL_BRIGHT * 1.55f * DpLookC();
        if (g_dpLook == 2) al *= 1.3f;
        if (al > 0.9f) al = 0.9f;
        {
            const float rot = FxRnd(kk + 37u) * 6.28f;
            const int cell = (int)(kk & 1u);
            DpSlAdd(p, size, rot, DPT_SMOKE, DPS_SMOKE0 + cell, 0, col, al, 0.0f, DPF_SIZE | DPF_FLOOR, 0, 0.0f);
            if (g_dpLook == 0) DpSlAdd(p, size, rot, DPT_SMOKE, DPS_CREST0 + cell, 1, kDpHiCol, DP_VOL_GLOW * al / DP_VOL_ALPHA, 0.0f,
                                       DPF_SIZE | DPF_FLOOR, 0, 0.0f);
        }
    }
}

// ---------------------------------------------------------------- B. her aura (PLAN 3; NOTES_v5 2-3)
static const float kDpHaloCol[3] = { 0.0f, 0.0f, 0.0f },  /* v3.27.2 v7: pure black */ kDpFringeCol[3] = { 0.085f, 0.025f, 0.200f };
#define DP_HALO_RMUL 1.35f
#define DP_HALO_BIAS 60.0f
static int g_dpHaloFl;  // v3.27.2: her cloak: DPF_FLOOR | DPF_CEIL (cut at her shoulder line); victims: 0
static void DpHaloSprite(const float p[3], float rad, float rot, float al, float fr, float floorY)
{
    float q[3];
    memcpy(q, p, sizeof q);
    if (q[1] > floorY - 0.75f * rad) q[1] = floorY - 0.75f * rad;  // no flat cut at the floor (Y down)
    if (fr > 0.0f && g_dpLook == 0)  // the faint violet fringe, just behind the black
        DpSlAdd(q, rad * 1.55f, rot + 3.0f, DPT_SMOKE, DPS_SOFT, 0, kDpFringeCol, fr, 0.0f, g_dpHaloFl, 2, DP_HALO_BIAS - 4.0f);
    DpSlAdd(q, rad, rot, DPT_SMOKE, DPS_HALO, 0, kDpHaloCol, al, 0.0f, g_dpHaloFl, 2, DP_HALO_BIAS);
}
// v3.27.2 (user item 4, answer "cloak + trailing hem"; PLAN 4): the cloak of death: the black glow only from her toes to her
// shoulders (head, neck, upper chest exposed), smooth (the halo cell has no holes), plus a hem round her feet that trails
// behind her when she walks, and fray pieces that come off it and sink into the pool
static float g_dpVel[2], g_dpVelPrev[3];
static int g_dpVelOk;
static void DpHerVelocity(const float pp[3])  // her smoothed horizontal velocity (0.25 s), from her position each frame
{
    const float dt = g_dpDt;
    if (g_dpVelOk && dt > 1e-4f && dt < 0.25f) {
        const float a = 1.0f - expf(-dt / 0.25f);
        float vx = (pp[0] - g_dpVelPrev[0]) / dt, vz = (pp[2] - g_dpVelPrev[2]) / dt;
        if (!(vx * vx + vz * vz < 1000.0f * 1000.0f)) vx = vz = 0.0f;  // a teleport (door, warp): no streak
        g_dpVel[0] += (vx - g_dpVel[0]) * a;
        g_dpVel[1] += (vz - g_dpVel[1]) * a;
    } else if (!g_dpVelOk) {
        g_dpVel[0] = g_dpVel[1] = 0.0f;
    }
    memcpy(g_dpVelPrev, pp, sizeof g_dpVelPrev);
    g_dpVelOk = 1;
}
static void DpTrail(float k, float o[2])  // the hem's trailing offset: -v x 0.45 s x k, at most 80 u (v7; was 0.30 s / 60 u)
{
    float x = -g_dpVel[0] * 0.45f * k, z = -g_dpVel[1] * 0.45f * k;
    const float l = sqrtf(x * x + z * z);
    if (l > 80.0f) x *= 80.0f / l, z *= 80.0f / l;
    o[0] = x, o[1] = z;
}
// v3.27.3 (user v3.27.2 item 3, re_depth.md 5.3): from a camera looking steeply down, the close-camera fade draws her
// without depth of her own, so cloak sprites behind her head / chest showed over them: fade those whose screen centre
// lies inside her projected head (28 u) or chest (34 u) circle, by how steeply the camera looks down (0.55 .. 0.80)
static float g_dpHeadDown, g_dpHeadC[2][3];  // this frame's steepness and the circles' screen x, y, radius (px)
static void DpHeadMaskFrame(void)
{
    float v[3], l, h[4];
    int i;
    g_dpHeadDown = 0.0f;
    if (!g_svEyeOk || !g_dpHerOk || !g_svVp) return;
    for (i = 0; i < 3; ++i) v[i] = g_dpHer[HB_HEAD][i] - g_svEye[i];
    l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(l > 1.0f)) return;
    g_dpHeadDown = FxSmooth(0.55f, 0.80f, v[1] / l);  // (Y is down: the camera above her looks along +y)
    for (i = 0; i < 2 && g_dpHeadDown > 0.0f; ++i) {
        GvXform(g_dpHer[i ? HB_CHEST : HB_HEAD], h);
        if (!GvScreen(h, g_svVp, &g_dpHeadC[i][0], &g_dpHeadC[i][1])) { g_dpHeadDown = 0.0f; break; }
        g_dpHeadC[i][2] = (i ? 34.0f : 28.0f) * g_svPxY / h[3];
    }
}
static float DpHeadMask(const float p[3])  // the alpha factor of a cloak sprite centred at p
{
    float h[4], x, y, k = 1.0f;
    int i;
    if (!(g_dpHeadDown > 0.0f)) return 1.0f;
    GvXform(p, h);
    if (!GvScreen(h, g_svVp, &x, &y)) return 1.0f;
    for (i = 0; i < 2; ++i) {
        const float dx = x - g_dpHeadC[i][0], dy = y - g_dpHeadC[i][1], d = sqrtf(dx * dx + dy * dy), r = g_dpHeadC[i][2];
        const float m = 1.0f - g_dpHeadDown * (1.0f - FxSmooth(0.9f * r, 1.8f * r, d));
        k = m < k ? m : k;
    }
    return k;
}
#define DP_FRAY_N 8
typedef struct { float anchor[3]; int cycle; } DpFraySlot;
static DpFraySlot g_dpFray[DP_FRAY_N];
static void DpHerHalo(int level, float floorY)
{
    // the cloak's bones (PBONES order) and its midpoint segments: no head / neck / chest; no head-neck, neck-chest,
    // neck-shoulder segments
    static const BYTE kCloak[HB_N] = { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    static const BYTE kCloakEdge[15] = { 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    const DpLook *L = &kDpLook[level];
    const float t = g_dpNow, Ha = L->haloA * (g_dpLook == 2 ? 1.3f : 1.0f);
    float foot[3];
    int i;
    // review DP3272-006: the cloak's upper edge = her shoulder line (the mean shoulder bone height above her floor): every
    // cloak quad is clipped there in its own plane, fading over 20 u below it, whatever the pose or camera
    g_dpCeilH = floorY - 0.5f * (g_dpHer[HB_LSH][1] + g_dpHer[HB_RSH][1]);
    g_dpHaloFl = DPF_FLOOR | DPF_CEIL;
    DpHeadMaskFrame();
    for (i = 0; i < HB_N; ++i) {
        float p[3], rad = kDpHaloR[i] * DP_HALO_RMUL;
        const float al = Ha * (0.85f + 0.15f * sinf(0.9f * t + 1.3f * (float)i));
        if (!kCloak[i]) continue;
        DpAway(g_dpHer[i], DpLerp(18.0f, 25.0f, FxRnd((DWORD)i * 31u + 7u)), p);
        if (i == HB_LSH || i == HB_RSH) rad *= 0.7f, p[1] += 6.0f;  // shoulders: smaller (v7 x 0.7) and 6 u lower (Y down)
        {
            const float hm = DpHeadMask(p);  // v3.27.3
            DpHaloSprite(p, rad, 0.7f * (float)i, al * hm, L->fringeA * al * hm / L->haloA, floorY);
        }
    }
    for (i = 0; i < 15; ++i) {  // the segment midpoints: a continuous rim
        float m[3], p[3];
        int k;
        const float al = Ha * 0.9f * (0.85f + 0.15f * sinf(0.9f * t + 1.7f * (float)i));
        if (!kCloakEdge[i]) continue;
        for (k = 0; k < 3; ++k) m[k] = 0.5f * (g_dpHer[kDpHerEdge[i][0]][k] + g_dpHer[kDpHerEdge[i][1]][k]);
        DpAway(m, DpLerp(18.0f, 25.0f, FxRnd((DWORD)i * 53u + 11u)), p);
        {
            const float hm = DpHeadMask(p);  // v3.27.3
            DpHaloSprite(p, 14.0f + 1.6f * kDpHerEdgeR[i], 0.7f * (float)(i + 20), al * hm, L->fringeA * al * hm / L->haloA, floorY);
        }
    }
    // the hem: 12 black puffs round her feet (not pushed: they cover her ankles), streaming behind her as she walks
    foot[0] = 0.5f * (g_dpHer[HB_LFOOT][0] + g_dpHer[HB_RFOOT][0]);
    foot[1] = floorY;
    foot[2] = 0.5f * (g_dpHer[HB_LFOOT][2] + g_dpHer[HB_RFOOT][2]);
    {   // v7 (review DP3272-ART-001): the hem sprites stretch along her trail on screen, x (1 + 2 min(1, speed / 150) x
        // min(1, 1.4 x the trail's on-screen share)): 3 at a walk seen side-on, round when she stands or walks along the view
        const float sp = sqrtf(g_dpVel[0] * g_dpVel[0] + g_dpVel[1] * g_dpVel[1]);
        g_dpStretchK = 1.0f;
        if (sp > 5.0f && g_svVp) {
            const float w0[3] = { foot[0], foot[1] - 20.0f, foot[2] },
                        w1[3] = { foot[0] - g_dpVel[0] / sp * 30.0f, foot[1] - 20.0f, foot[2] - g_dpVel[1] / sp * 30.0f };
            float h0[4], h1[4], x0, y0, x1, y1;
            GvXform(w0, h0), GvXform(w1, h1);
            if (GvScreen(h0, g_svVp, &x0, &y0) && GvScreen(h1, g_svVp, &x1, &y1)) {
                // the direction in the sprites' rx / ry space (pixels / g_svPxX, g_svPxY), and its share of 30 u on screen
                const float ux = (x1 - x0) / g_svPxX * h0[3], uy = (y1 - y0) / g_svPxY * h0[3], ul = sqrtf(ux * ux + uy * uy);
                if (ul > 0.05f * 30.0f && _finite(ul)) {
                    const float sh = ul / 30.0f;
                    g_dpStretchD[0] = ux / ul, g_dpStretchD[1] = uy / ul;
                    g_dpStretchK = 1.0f + 2.0f * (sp < 150.0f ? sp / 150.0f : 1.0f) * (1.4f * sh < 1.0f ? 1.4f * sh : 1.0f);
                }
            }
        }
    }
    for (i = 0; i < 12; ++i) {
        const DWORD k = (DWORD)i * 7349u + 101u;
        const float a = 6.2831853f * ((float)i / 12.0f + 0.1f * FxRnd(k)), r = DpLerp(18.0f, 34.0f, FxRnd(k + 1u));
        const float h = DpLerp(6.0f, 34.0f, FxRnd(k + 2u)), hf = (h - 6.0f) / 28.0f;
        float p[3], tr[2];
        DpTrail(0.4f + 0.6f * hf, tr);
        p[0] = foot[0] + r * cosf(a + 0.25f * sinf(0.5f * t + (float)i)) + tr[0];
        p[1] = foot[1] - h;
        p[2] = foot[2] + r * sinf(a + 0.25f * sinf(0.5f * t + (float)i)) + tr[1];
        {   // v7: sprite radius 12-19 (the judged preview; was 26-38), centre at least 0.75 radius high
            const float rad = DpLerp(12.0f, 19.0f, FxRnd(k + 3u));
            if (p[1] > foot[1] - 0.75f * rad) p[1] = foot[1] - 0.75f * rad;
            DpSlAdd(p, rad, 0.9f * (float)i, DPT_SMOKE, DPS_HALO, 0, kDpHaloCol,
                    Ha * DpLerp(0.55f, 0.9f, FxRnd(k + 4u)) * (0.80f + 0.20f * sinf(0.8f * t + (float)i)), 0.0f, DPF_CEIL, 2, 0.0f);
        }
    }
    g_dpStretchK = 1.0f;
    for (i = 0; i < DP_FRAY_N; ++i) {  // the fray: pieces off the hem that sink to the floor and fade (1.2-2 s, hashed)
        const float life = DpLerp(1.2f, 2.0f, FxRnd((DWORD)i * 3301u + 7u)), tt = t + FxRnd((DWORD)i * 9133u + 3u) * life;
        const int cyc = (int)floorf(tt / life);
        const float age = tt - (float)cyc * life, f = age / life;
        const DWORD k = (DWORD)i * 613u + (DWORD)cyc * 2477u;
        const float env = FxSmooth(0.0f, 0.06f, f) * (1.0f - FxSmooth(0.30f, 1.0f, f));  // v7: in 0-6 %, out 30-100 %
        float p[3];
        if (g_dpFray[i].cycle != cyc) {  // born on the hem, where it is now
            const float a = 6.2831853f * FxRnd(k), r = DpLerp(18.0f, 34.0f, FxRnd(k + 1u));
            float tr[2];
            DpTrail(1.0f, tr);
            g_dpFray[i].cycle = cyc;
            g_dpFray[i].anchor[0] = foot[0] + r * cosf(a) + tr[0];
            g_dpFray[i].anchor[1] = foot[1] - DpLerp(18.0f, 34.0f, FxRnd(k + 2u));
            g_dpFray[i].anchor[2] = foot[2] + r * sinf(a) + tr[1];
        }
        p[0] = g_dpFray[i].anchor[0];
        p[1] = g_dpFray[i].anchor[1] + (floorY - 6.0f - g_dpFray[i].anchor[1]) * f;  // sinking to the floor (Y down)
        p[2] = g_dpFray[i].anchor[2];
        DpSlAdd(p, DpLerp(12.0f, 22.0f, FxRnd(k + 3u)), 0.7f * (float)i, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 0,
                kDpHaloCol, Ha * (0.65f / 0.92f) * env, 0.0f, DPF_CEIL, 1, 0.0f);  // v7: radius 12-22, alpha 0.65 at Strong
    }
    g_dpHaloFl = 0, g_dpCeilH = 1.0e9f;
}
// ---------------------------------------------------------------- v3.28 TRAIL: the black-fire trail (user v3.27.3 item 7;
// staging/v328_spirit/c_trail/DESIGN.md variant A, NOTES.md). A ring of path nodes = her footprints: every
// S = clamp(0.32 x speed, 40, 120) u of path a node is laid at the exact point where the distance crossed S, its birth
// time interpolated inside the frame (the same nodes at any frame rate), 7 u left / right of the path by turns. Each node
// is a hashed seed (its serial) for its black flame tongues (ember-red / violet rim), a smouldering footprint decal,
// embers, ash and a dark heat wisp, all aged in real seconds; nothing new while she stands; a new room clears the ring.
#define DP_TR_N     32
#define DP_TR_LIFE  2.6f   // the oldest part of a node (its footprint) is gone by then
#define DP_TR_VMIN  30.0f  // u/s: below this no path is counted (standing, turning on the spot)
#ifndef DP_TR_V8B   // v3.28 TRAIL v8b (the OTS pass, NOTES.md): a V-shaped wake spreading beyond her silhouette, taller tongues,
#define DP_TR_V8B 1 // black smoke columns off the nearest nodes, embers rising past her hips, a brighter edge on the newest
#endif              // tongues, a wider and longer wake at a run. 0 = the v8 look
#define DpTrV(v8, v8b) (DP_TR_V8B ? (v8b) : (v8))
#ifndef DP_TR_V8C   // v3.28 TRAIL v8c (review ART-T1-001): the wake in frame from the OTS camera: tongues wider out and carried
#define DP_TR_V8C 1 // beside her a moment, the newest taller, embers born higher and wider, the smoke kept below her waist (no
#endif              // smoke at head height), the low heat wisps darker. 0 = v8b (needs DP_TR_V8B)
#define DpTrC(v8b, v8c) (DP_TR_V8B && DP_TR_V8C ? (v8c) : (v8b))
typedef struct { float p[3], t0, sp, dir[2], seg; DWORD id; } DpTrNode;  // p: on her floor (Y down); seg: its stride
static DpTrNode g_dpTr[DP_TR_N];
static int g_dpTrHead, g_dpTrCount, g_dpTrPrevOk;
static DWORD g_dpTrId;
static float g_dpTrAcc, g_dpTrPrev[3];
static LONG g_dpTrFrame = -1;
static void DpTrailReset(void) { g_dpTrCount = 0, g_dpTrAcc = 0.0f, g_dpTrPrevOk = 0; }
static float DpTrSpacing(float sp) { const float s = 0.32f * sp; return s < 40.0f ? 40.0f : s > DpTrV(120.0f, 70.0f) ? DpTrV(120.0f, 70.0f) : s; }  // (v8b: 70 at a run: no gap behind her)
// once a frame: lay the nodes her path crossed since the last frame (at most 6; a jump faster than 1200 u/s or 80 u
// up / down = a teleport: nothing laid). canLay: she is drawn this frame
static void DpTrailStep(const float pp[3], int canLay)
{
    const float dt = g_dpDt, sp = sqrtf(g_dpVel[0] * g_dpVel[0] + g_dpVel[1] * g_dpVel[1]);
    float dx, dy, dz, d;
    int n = 0;
    if (g_dpTrFrame == g_fxFrame) return;
    g_dpTrFrame = g_fxFrame;
    if (!g_dpTrPrevOk) {
        memcpy(g_dpTrPrev, pp, sizeof g_dpTrPrev);
        g_dpTrPrevOk = 1, g_dpTrAcc = 0.0f;
        return;
    }
    dx = pp[0] - g_dpTrPrev[0], dy = pp[1] - g_dpTrPrev[1], dz = pp[2] - g_dpTrPrev[2];
    d = sqrtf(dx * dx + dz * dz);
    memcpy(g_dpTrPrev, pp, sizeof g_dpTrPrev);
    if (!(d <= 1200.0f * dt + 5.0f) || !(fabsf(dy) <= 80.0f)) { g_dpTrAcc = 0.0f; return; }  // a teleport
    if (!canLay || sp < DP_TR_VMIN || !(d > 1e-3f)) return;
    {
        const float S = DpTrSpacing(sp), ux = g_dpVel[0] / sp, uz = g_dpVel[1] / sp;
        g_dpTrAcc += d;
        while (g_dpTrAcc >= S && n < 6) {
            DpTrNode *q = &g_dpTr[g_dpTrHead];
            float back, side;
            g_dpTrAcc -= S;
            back = g_dpTrAcc / d;  // the crossing lies this fraction of the frame's step before her
            if (back > 1.0f) back = 1.0f;
            side = (g_dpTrId & 1u) ? 7.0f : -7.0f;
            q->p[0] = pp[0] - dx * back - uz * side;  // (-uz, ux): her left / right
            q->p[1] = pp[1] - dy * back;
            q->p[2] = pp[2] - dz * back + ux * side;
            q->t0 = g_dpNow - dt * back;
            q->sp = sp, q->dir[0] = ux, q->dir[1] = uz, q->seg = S, q->id = g_dpTrId++;
            g_dpTrHead = (g_dpTrHead + 1) % DP_TR_N;
            if (g_dpTrCount < DP_TR_N) ++g_dpTrCount;
            ++n;
        }
        if (n >= 6) g_dpTrAcc = 0.0f;
    }
}
// the clear tunnel of the volume for the trail: a sprite on the eye rays to her feet / pelvis fades (OTS: the trail
// points at the camera; it flanks her and rises behind her, it does not cover her)
static float DpTrTunnel(const float p[3], float size, const float pp[3])
{
    float aim[3], e;
    if (!g_svEyeOk) return 1.0f;
    memcpy(aim, pp, sizeof aim);
    aim[1] -= 12.0f;  // her feet (Y down)
    e = DpSegDist(p, g_svEye, aim);
    if (g_dpHerOk) {
        const float e2 = DpSegDist(p, g_svEye, g_dpHer[HB_PELVIS]);
        e = e2 < e ? e2 : e;
    }
    return FxSmooth(0.10f * size + 6.0f, 0.60f * size + 26.0f, e);
}
static float DpTrNear(const float p[3])  // the trail's own near fade: gone within 60 u of the eye, whole from 140 u
{
    float d;
    DpNearK(p, &d);
    return FxSmooth(60.0f, 140.0f, d);
}
// a trail sprite of world half length hl at p that would fill more than 45-70 % of the screen height fades out (a running
// trail reaches under the camera: no giant smears)
static float DpTrBig(const float p[3], float hl)
{
    float h[4];
    GvXform(p, h);
    if (!(h[3] > GV_NEAR_W) || !g_svVp) return 0.0f;
    return 1.0f - FxSmooth(DpTrV(0.45f, 0.60f), DpTrV(0.70f, 0.90f), 2.0f * hl * g_svPxY / h[3] / (float)g_svVp->h);
}
// a tongue: the FXX_FLAME teardrop, camera facing, its foot (20 % up the quad: the round bottom is under the floor) at `base`, its tip at `tip`, half width hw;
// stretched along the foot-tip line on screen (at least 0.45 x its world length: from above it does not vanish)
static void DpTrFlame(const float base[3], const float tip[3], float hw, const float rgb[3], float a, int add, int prio, float bias, int cell)
{   // cell < 0: FXX_FLAME, its foot 20 % up the quad; else the FXX_SMOKE cell (ragged, with holes), its foot 15 % up
    const float foot = cell < 0 ? 0.20f : 0.15f;
    float hb[4], ht[4], xb, yb, xt, yt, ex, ey, el, wl, c[3];
    int k;
    if (!(a > 0.004f) || !(hw > 0.5f)) return;
    a *= DpTrNear(base);
    GvXform(base, hb), GvXform(tip, ht);
    if (!GvScreen(hb, g_svVp, &xb, &yb) || !GvScreen(ht, g_svVp, &xt, &yt)) return;
    ex = (xt - xb) / g_svPxX * hb[3], ey = (yt - yb) / g_svPxY * hb[3];  // in world units at her depth (the sprite's rx / ry space)
    el = sqrtf(ex * ex + ey * ey);
    wl = sqrtf((tip[0] - base[0]) * (tip[0] - base[0]) + (tip[1] - base[1]) * (tip[1] - base[1]) + (tip[2] - base[2]) * (tip[2] - base[2]));
    if (!(el > 0.05f * wl) || !_finite(el)) ex = 0.0f, ey = -1.0f, el = 1.0f;  // seen along its axis: upright
    ex /= el, ey /= el;
    if (el < 0.45f * wl) el = 0.45f * wl;
    a *= DpTrBig(base, 0.5f * wl / (1.0f - foot));
    if (!(a > 0.004f)) return;
    for (k = 0; k < 3; ++k) c[k] = base[k] + (tip[k] - base[k]) * ((0.5f - foot) / (1.0f - foot));
    g_dpStretchK = el / (1.0f - foot) * 0.5f / hw;  // the quad's half length / half width
    if (g_dpStretchK < 1.0f) g_dpStretchK = 1.0f;
    g_dpStretchD[0] = ex, g_dpStretchD[1] = ey;
    if (cell < 0) {  // v3.28: an animated flipbook frame of the fire atlas (18 frames a second, its phase hashed from where it burns)
        const DWORD hh = FxHash((DWORD)(int)floorf(base[0] * 0.25f) * 73856093u ^ (DWORD)(int)floorf(base[2] * 0.25f) * 19349663u);
        cell = ((int)floorf(g_dpNow * 18.0f) + (int)(hh % 28u)) % 28;
        DpSlAdd(c, hw, atan2f(ex, -ey), DPT_FLAME, cell, add, rgb, a, 0.0f, 0, prio, bias);
    } else {
        DpSlAdd(c, hw, atan2f(ex, -ey), DPT_SMOKE, cell, add, rgb, a, 0.0f, 0, prio, bias);  // (0, -1) (the tip row) -> (ex, ey)
    }
    g_dpStretchK = 1.0f;
}
// a footprint: a floor quad (half length hl along ax, half width hw) 1.5 u above her floor, each corner projected (its
// own depth); FXX_SMOKE `cell`; 6 vertices, 0 when a corner is behind the near plane
static int DpTrDecal(SpVtx *v, const float c[3], const float ax[2], float hl, float hw, int cell, DWORD col)
{
    const float u0 = 0.25f * (float)(cell & 3) + 0.5f / 1024.0f, v0 = 0.5f * (float)(cell >> 2) + 0.5f / 512.0f;
    const float u1 = u0 + 0.25f - 1.0f / 1024.0f, v1 = v0 + 0.5f - 1.0f / 512.0f;
    SpVtx q[4];
    int m;
    for (m = 0; m < 4; ++m) {
        const float sa = (m & 1) ? 1.0f : -1.0f, sb = (m & 2) ? 1.0f : -1.0f;
        const float w[3] = { c[0] + sa * hl * ax[0] - sb * hw * ax[1], c[1] - 1.5f, c[2] + sa * hl * ax[1] + sb * hw * ax[0] };
        float h[4], x, y;
        GvXform(w, h);
        if (!GvScreen(h, g_svVp, &x, &y)) return 0;
        q[m].x = x - 0.5f, q[m].y = y - 0.5f, q[m].z = SvZH(h) * g_dpZMul, q[m].rhw = 1.0f / h[3];
        q[m].c = col, q[m].u = sa > 0.0f ? u1 : u0, q[m].v = sb > 0.0f ? v1 : v0;
    }
    v[0] = q[0], v[1] = q[1], v[2] = q[2], v[3] = q[1], v[4] = q[3], v[5] = q[2];
    return 6;
}
static DWORD DpTrCol(const float rgb[3], float a, int add)  // a vertex colour as DpSlDraw makes it
{
    if (add) {
        const float k = a * g_svMul;
        return 0xFF000000u | (DWORD)(FxSat(rgb[0] * k) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(rgb[1] * k) * 255.0f + 0.5f) << 8 |
               (DWORD)(FxSat(rgb[2] * k) * 255.0f + 0.5f);
    } else {
        const float q = FxSat(a * g_svMul);
        return (DWORD)(q * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(rgb[0]) * q * 255.0f + 0.5f) << 16 |
               (DWORD)(FxSat(rgb[1]) * q * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(rgb[2]) * q * 255.0f + 0.5f);
    }
}
// v3.29.1 (user, v3.28 test item 1: "make the puffy orange trails thin and black-purple and their embers purplish"):
// every ember-orange colour of v8c is now a violet / purple (rims, embers, cracks, the newest edge, the ground glow)
static const float kDpTrEmber[3] = { 0.62f, 0.10f, 0.78f }, kDpTrViolet[3] = { 0.36f, 0.05f, 0.62f };  // the tongues' rim: new / old
static const float kDpTrSpark[2][3] = { { 0.78f, 0.30f, 1.00f }, { 0.55f, 0.12f, 0.80f } };          // embers: lilac / purple
static const float kDpTrAsh[3] = { 0.025f, 0.018f, 0.030f }, kDpTrCrack[3] = { 0.60f, 0.14f, 0.85f };
static const float kDpTrHot[3] = { 0.80f, 0.28f, 1.00f };  // v8b: the newest tongues' edge
static const float kDpTrGlow[2][3] = { { 0.45f, 0.08f, 0.70f }, { 0.25f, 0.04f, 0.45f } };  // the ground glow: new / old
#define DP_TR_THIN 0.60f  // v3.29.1: the tongues' width x 0.6 (thin, not puffy)
static const float kDpTrSv[3] = { 0.30f, 0.30f, 0.95f }, kDpTrSvHot[3] = { 0.55f, 0.65f, 1.0f };     // Spirit Vision
// a tongue in the frame's look: full: a black core over its rim (ember red at f = 0, cooling to violet), drawn just before
// it; Thermal / night vision: the core only (night vision alpha x nvA); Spirit Vision: an additive blue tongue in a halo
static void DpTrTongue(const float base[3], const float tip[3], float hw, float f, float al, float nvA, int cell, float rim, float hot)
{
    float a;
    if (g_dpLook == 3) {
        DpTrFlame(base, tip, hw * 1.25f, kDpTrSv, 0.15f * al, 1, 1, 0.0f, cell);
        DpTrFlame(base, tip, hw, kDpTrSvHot, 0.20f * al, 1, 1, 0.0f, cell);
        return;
    }
    if (g_dpLook == 0 && rim > 0.0f) {
        float rc[3], tip2[3];
        int c;
        const float m = FxSmooth(0.30f, 0.85f, f) * (1.0f - hot);  // hot (v8b, the newest nodes): a brighter ember edge
        for (c = 0; c < 3; ++c)
            rc[c] = DpLerp(DpLerp(kDpTrEmber[c], kDpTrViolet[c], m), kDpTrHot[c], hot), tip2[c] = base[c] + (tip[c] - base[c]) * 1.08f;
        DpTrFlame(base, tip2, hw * 1.25f * DP_TR_THIN, rc, 0.65f * rim * al * (1.0f + 0.5f * hot), 0, 1, 40.0f - 3.0f, cell);
    }
    a = al * nvA;
    DpTrFlame(base, tip, hw * DP_TR_THIN, kDpHaloCol, a > 0.95f ? 0.95f : a, 0, 2, 40.0f, cell);
}
static SpVtx g_dpTrDecV[3][DP_TR_N * 6];  // the footprints: glow (additive), print (black), cracks (additive)
// the trail's sprites (into the DP list) and its footprints (drawn here, before the list); returns the triangles drawn
// v8b: her slipstream carries a sprite born on node q along her path for a while: 0.85 x speed x tau x (1 - e^(-ta / tau)) u
// (closed form: any frame rate); at a run embers and smoke stay beside her long enough to be seen from behind her
static void DpTrCarry(const DpTrNode *q, float ta, float tau, float p[3])
{
    const float c = DP_TR_V8B && ta > 0.0f ? 0.85f * q->sp * tau * (1.0f - expf(-ta / tau)) : 0.0f;
    p[0] += q->dir[0] * c, p[2] += q->dir[1] * c;
}
static int DpHerTrail(void *dev, int level, const float pp[3])
{
    const DpLook *L = &kDpLook[level];
    const float t = g_dpNow, lvA = L->haloA / 0.92f, nvA = g_dpLook == 2 ? 1.3f : 1.0f;  // Weak 0.71; night vision x 1.3 (as the cloak)
    const int strong = level >= 2, sv = g_dpLook == 3;
    int i, nd[3] = { 0, 0, 0 }, tris = 0;
    if (!g_svVp) return 0;
    for (i = 0; i < g_dpTrCount; ++i) {
        const DpTrNode *q = &g_dpTr[(g_dpTrHead - 1 - i + 2 * DP_TR_N) % DP_TR_N];
        const float age = t - q->t0, Iw = FxSmooth(DP_TR_VMIN, 120.0f, q->sp), R = FxSmooth(250.0f, 600.0f, q->sp);
        const float px = -q->dir[1], pz = q->dir[0];  // her left at the node
        const float hot = DpTrV(0.0f, 1.0f - FxSmooth(0.25f, 0.9f, age));
        int j, nT, nE, nA, nW;
        if (age > DP_TR_LIFE) break;  // (newest first)
        if (age < 0.0f) continue;
        nT = (strong ? 5 : 3) + (int)(DpTrV(2.0f, 3.0f) * R + 0.5f);
        nE = (strong ? DpTrV(4, 6) : DpTrV(2, 3)) + (int)(DpTrV(2.0f, 3.0f) * R + 0.5f);
        nA = (strong ? 2 : 1) + (int)(R + 0.5f);
        nW = 1 + (int)(R + 0.5f);
        {   // the smouldering footprint: black, gone over 1.0-2.6 s; glowing cracks cooling over 0.15-1.5 s
            const DWORD k = q->id * 911u + 5u;
            const float rot = (FxRnd(k) - 0.5f) * 0.35f, cs = cosf(rot), sn = sinf(rot);
            const float ax[2] = { q->dir[0] * cs - q->dir[1] * sn, q->dir[0] * sn + q->dir[1] * cs };
            const float hl = 15.0f * (1.0f + 0.25f * R), hw = 7.5f * (1.0f + 0.15f * R), nk = DpNearK(q->p, NULL);
            const float ab = 0.80f * lvA * Iw * nk * FxSmooth(0.0f, 0.10f, age) * (1.0f - FxSmooth(1.0f, DP_TR_LIFE, age));
            const float ag = 0.90f * lvA * Iw * nk * FxSmooth(0.0f, 0.08f, age) * (1.0f - FxSmooth(0.15f, 1.5f, age)) *
                             (0.75f + 0.25f * sinf(6.1f * t + 6.2831853f * FxRnd(k + 1u)));
            // the ground glow under the fire (the black tongues stand against it): ember red cooling to violet over 1.6 s
            const float aw = 0.70f * lvA * Iw * nk * FxSmooth(0.05f, 0.20f, age) * (1.0f - FxSmooth(0.6f, 1.6f, age));
            if ((g_dpLook == 0 || sv) && aw > 0.004f) {
                float gc[3];
                int c;
                const float m = FxSmooth(0.4f, 1.4f, age);
                for (c = 0; c < 3; ++c) gc[c] = sv ? kDpTrSv[c] : DpLerp(kDpTrGlow[0][c], kDpTrGlow[1][c], m);
                nd[0] += DpTrDecal(g_dpTrDecV[0] + nd[0], q->p, q->dir, 30.0f * (1.0f + 0.3f * R), 22.0f * (1.0f + 0.3f * R), DPS_SOFT,
                                   DpTrCol(gc, aw * (sv ? 0.5f : 1.0f), 1));
            }
            if (!sv && ab > 0.004f)
                nd[1] += DpTrDecal(g_dpTrDecV[1] + nd[1], q->p, ax, hl, hw, DPS_HALO, DpTrCol(kDpTrAsh, ab * nvA > 0.95f ? 0.95f : ab * nvA, 0));
            if (g_dpLook != 1 && ag > 0.004f)  // (Thermal: no light)
                nd[2] += DpTrDecal(g_dpTrDecV[2] + nd[2], q->p, ax, hl * 0.85f, hw * 0.8f, DPS_CREST0 + (int)(q->id & 1u),
                                   DpTrCol(sv ? kDpTrSv : g_dpLook == 2 ? kDpNvCol : kDpTrCrack, ag * (sv ? 0.35f : g_dpLook == 2 ? 0.45f : 1.0f), 1));
        }
        for (j = 0; j < nT; ++j) {  // the black flame tongues
            const DWORD k = q->id * 7919u + (DWORD)j * 131u + 17u;
            const float u = FxRnd(k), lat0 = (FxRnd(k + 1u) < 0.5f ? -1.0f : 1.0f) * (DpTrC(DpTrV(10.0f, 14.0f), 18.0f) + (DpTrV(16.0f, 14.0f) + DpTrC(DpTrV(8.0f, 10.0f), 14.0f) * R) * FxRnd(k + 12u));  // a wake flanking her path
            const float ta = age - (DpTrV(0.06f, 0.03f) + DpTrV(0.22f, 0.12f) * FxRnd(k + 2u)) * (1.0f - 0.6f * R);
            const float life = DpLerp(0.9f, 1.8f, FxRnd(k + 3u)) * (1.0f - DpTrV(0.25f, 0.0f) * R);
            const float H0 = (DpTrV(30.0f, 44.0f) + DpTrV(50.0f, 66.0f) * FxRnd(k + 4u) * FxRnd(k + 4u)) * (1.0f + DpTrV(0.45f, 0.25f) * R)  // (many short, a few tall)
                              * Iw * (strong ? 1.0f : 0.85f) * (1.0f + DpTrC(0.0f, 0.4f) * hot);  // (v8c: the newest taller)
            const float wr = DpLerp(0.50f, 0.68f, FxRnd(k + 5u)), ph = 6.2831853f * FxRnd(k + 6u);
            float f, grow, burn, H, hw, lat, base[3], tip[3], al, lean, sway, tun;
            if (ta < 0.0f || ta >= life) continue;
            f = ta / life;
            grow = FxSmooth(0.0f, 0.10f, f), burn = 1.0f - FxSmooth(0.35f, 1.0f, f);
            H = H0 * grow * (0.35f + 0.65f * burn) * (1.0f + 0.16f * sinf(8.7f * t + ph) + 0.09f * sinf(14.3f * t + 2.1f * ph));
            hw = 0.5f * H0 * wr * grow * (0.60f + 0.40f * burn);
            lat = lat0 + (lat0 < 0.0f ? -1.0f : 1.0f) * (DpTrV(9.0f, 55.0f) + DpTrV(9.0f, 150.0f) * R) * ta;  // the wake fans out (v8b: a V)
            base[0] = q->p[0] - q->dir[0] * u * q->seg + px * lat;
            base[1] = q->p[1] - 16.0f * (H0 / 40.0f) * FxSmooth(0.5f, 1.0f, f);  // it tears upward as it dies (Y down)
            base[2] = q->p[2] - q->dir[1] * u * q->seg + pz * lat;
            DpTrCarry(q, ta, DpTrC(0.08f, 0.20f), base);
            lean = (0.25f + 0.25f * R) * H * (1.0f - f), sway = H * ((FxRnd(k + 9u) - 0.5f) * 0.4f + 0.12f * sinf(5.3f * t + ph));
            tip[0] = base[0] - q->dir[0] * lean + px * sway;
            tip[1] = base[1] - H;
            tip[2] = base[2] - q->dir[1] * lean + pz * sway;
            al = 0.88f * lvA * FxSmooth(0.0f, 0.06f, f) * (1.0f - FxSmooth(0.62f, 1.0f, f));
            tun = 0.4f + 0.6f * DpTrTunnel(base, hw * 2.0f, pp);
            {   // two pieces: a ragged smoke body (0.62 of the height) and a narrower teardrop tongue bent sideways by a fast flicker
                const float bend = H * (0.10f + 0.10f * FxRnd(k + 10u)) * sinf(7.1f * t + 1.7f * ph) * (FxRnd(k + 11u) < 0.5f ? -1.0f : 1.0f);
                float tb[3], ub[3], ut[3];
                int c;
                for (c = 0; c < 3; ++c) tb[c] = base[c] + (tip[c] - base[c]) * 0.62f, ub[c] = base[c] + (tip[c] - base[c]) * 0.30f, ut[c] = tip[c];
                ub[0] += px * 0.4f * bend, ub[2] += pz * 0.4f * bend, ut[0] += px * bend, ut[2] += pz * bend;
                DpTrTongue(base, tb, 1.15f * hw, f, al * tun, nvA, DPS_SMOKE0 + (int)(k & 1u), 1.0f, hot);  // ragged body, glowing edge
                DpTrTongue(ub, ut, 0.62f * hw, f, al * tun, nvA, -1, DpTrV(0.6f, 0.35f), hot);                        // the tongue
            }
            {   // the lick: a small tongue torn off the top, born again every 0.22-0.34 s (a hashed place per cycle), rising
                // from 0.55 to 1.05 of the tongue and shrinking as it goes
                const float P = DpLerp(0.22f, 0.34f, FxRnd(k + 7u)), x = ta / P + FxRnd(k + 8u), g = x - floorf(x);
                const DWORD kc = k * 31u + (DWORD)(int)floorf(x) * 977u;
                const float Hl = 0.45f * H * (1.0f - 0.6f * g), side = (FxRnd(kc) - 0.5f) * 0.5f * H, ga = FxSmooth(0.0f, 0.2f, g) * (1.0f - FxSmooth(0.5f, 1.0f, g));
                float lb[3], lt[3];
                int c;
                for (c = 0; c < 3; ++c) lb[c] = base[c] + (tip[c] - base[c]) * (0.55f + 0.5f * g);
                lb[0] += px * side, lb[2] += pz * side;
                lt[0] = lb[0] - q->dir[0] * 0.3f * lean + px * 0.5f * side, lt[1] = lb[1] - Hl, lt[2] = lb[2] - q->dir[1] * 0.3f * lean + pz * 0.5f * side;
                DpTrTongue(lb, lt, 0.6f * hw * (1.0f - 0.5f * g), f, 0.9f * ga * al * tun, nvA, -1, 0.0f, 0.0f);  // black only
            }
        }
        if (g_dpLook != 1)  // (Thermal: no light)
            for (j = 0; j < nE; ++j) {  // embers: rising, drifting back, curling, flickering
                const DWORD k = q->id * 4567u + (DWORD)j * 197u + 29u;
                const float u = FxRnd(k), lat1 = (2.0f * FxRnd(k + 1u) - 1.0f) * (10.0f + 12.0f * R);
                const float ta = age - (DpTrV(0.10f, 0.05f) + DpTrV(0.50f, 0.35f) * FxRnd(k + 2u)) * (1.0f - 0.6f * R);
                const float life = (DpTrV(1.0f, 1.3f) + 0.9f * FxRnd(k + 3u)) * (1.0f - DpTrV(0.0f, 0.4f) * R);
                const float rise = DpLerp(DpTrC(DpTrV(60.0f, 120.0f), 70.0f), DpTrC(DpTrV(120.0f, 220.0f), 140.0f), FxRnd(k + 4u)) * (1.0f + 0.3f * R), r = DpLerp(1.4f, 2.6f, FxRnd(k + 5u));
                const float ph = 6.2831853f * FxRnd(k + 6u);
                float f, p[3], b, cu, lat;
                if (ta < 0.0f || ta >= life) continue;
                f = ta / life;
                lat = DP_TR_V8B ? (FxRnd(k + 8u) < 0.5f ? -1.0f : 1.0f) * (DpTrC(14.0f, 24.0f) + DpTrC(20.0f, 26.0f) * FxRnd(k + 1u) + (33.0f + 90.0f * R) * ta) : lat1;  // v8b: the wake
                cu = 9.0f * sinf(3.1f * t + ph) * f + lat;
                p[0] = q->p[0] - q->dir[0] * (u * q->seg + 14.0f * f) + px * cu;
                p[1] = q->p[1] - DpTrC(6.0f, 30.0f + 40.0f * FxRnd(k + 9u)) - rise * (1.0f - (1.0f - f) * (1.0f - f));  // (v8c: born at the tongues' tips)
                p[2] = q->p[2] - q->dir[1] * (u * q->seg + 14.0f * f) + pz * cu;
                DpTrCarry(q, ta, DpTrC(0.30f, 0.35f), p);
                b = 1.8f * DpTrNear(p) * lvA * FxSmooth(0.0f, 0.08f, f) * (1.0f - FxSmooth(0.55f, 1.0f, f)) * (0.6f + 0.4f * sinf(19.0f * t + ph));
                DpSlAdd(p, 4.0f * r, ph, DPT_ATLAS, 10, 1, sv ? kDpTrSvHot : g_dpLook == 2 ? kDpNvCol : kDpTrSpark[FxRnd(k + 7u) < 0.75f ? 0 : 1],
                        b, 1.3f * g_svUnit, 0, 1, 0.0f);
            }
        if (!sv)
            for (j = 0; j < nA; ++j) {  // ash: black flakes rising slowly, tumbling
                const DWORD k = q->id * 2711u + (DWORD)j * 61u + 41u;
                const float u = FxRnd(k), lat = (2.0f * FxRnd(k + 1u) - 1.0f) * (10.0f + 10.0f * R);
                const float ta = age - (0.20f + 0.60f * FxRnd(k + 2u)), life = 1.4f + 1.0f * FxRnd(k + 3u);
                const float rise = DpLerp(40.0f, 90.0f, FxRnd(k + 4u)), sz = DpLerp(2.4f, 4.2f, FxRnd(k + 5u)), ph = 6.2831853f * FxRnd(k + 6u);
                const float spin = (FxRnd(k + 7u) < 0.5f ? -1.0f : 1.0f) * (2.0f + 3.0f * FxRnd(k + 8u));
                float f, p[3], al, cu;
                if (ta < 0.0f || ta >= life) continue;
                f = ta / life;
                cu = lat + 12.0f * sinf(1.7f * t + ph) * f;
                p[0] = q->p[0] - q->dir[0] * (u * q->seg + 10.0f * f) + px * cu;
                p[1] = q->p[1] - 8.0f - rise * (1.0f - (1.0f - f) * (1.0f - f));
                p[2] = q->p[2] - q->dir[1] * (u * q->seg + 10.0f * f) + pz * cu;
                DpTrCarry(q, ta, 0.25f, p);
                al = 0.85f * DpTrNear(p) * lvA * nvA * FxSmooth(0.0f, 0.10f, f) * (1.0f - FxSmooth(0.60f, 1.0f, f)) * DpTrTunnel(p, sz, pp);
                DpSlAdd(p, 1.6f * sz, ph + spin * t, DPT_ATLAS, 7 + (int)(k & 1u), 0, kDpTrAsh, al > 0.95f ? 0.95f : al, 1.0f * g_svUnit, 0, 1, 40.0f);
            }
        for (j = 0; j < nW; ++j) {  // dark heat wisps: smoke stretched upward, rising from the fire (what reads from OTS)
            const DWORD k = q->id * 3203u + (DWORD)j * 89u + 53u;
            const float u = FxRnd(k), lat = (2.0f * FxRnd(k + 1u) - 1.0f) * (8.0f + 10.0f * R);
            const float ta = age - (0.15f + 0.30f * FxRnd(k + 2u)), life = 1.6f + 0.8f * FxRnd(k + 3u);
            const float rise = DpLerp(DpTrC(90.0f, 50.0f), DpTrC(150.0f, 80.0f), FxRnd(k + 4u)) * (1.0f + 0.25f * R), r0 = DpLerp(16.0f, 24.0f, FxRnd(k + 5u));  // (v8c: below her waist)
            const float ph = 6.2831853f * FxRnd(k + 6u);
            float f, p[3], al, rad, col[3];
            int c;
            if (ta < 0.0f || ta >= life) continue;
            f = ta / life;
            rad = r0 * (1.0f + 0.8f * f);
            p[0] = q->p[0] - q->dir[0] * u * q->seg + px * (lat + 8.0f * sinf(1.3f * t + ph));
            p[1] = q->p[1] - 28.0f - rise * powf(f, 0.8f);
            p[2] = q->p[2] - q->dir[1] * u * q->seg + pz * (lat + 8.0f * sinf(1.3f * t + ph));
            al = DpTrNear(p) * DpTrBig(p, 1.7f * rad) * lvA * FxSmooth(0.0f, 0.15f, f) * (1.0f - FxSmooth(0.40f, 1.0f, f));
            g_dpStretchK = 1.7f, g_dpStretchD[0] = 0.0f, g_dpStretchD[1] = 1.0f;  // upright (screen y)
            if (sv) {
                DpSlAdd(p, rad, ph, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 1, kDpTrSv, 0.05f * al, 0.0f, DPF_SIZE, 1, 0.0f);
            } else {
                for (c = 0; c < 3; ++c) col[c] = kDpTrAsh[c] * DpLookC();  // near black smoke
                al *= DpTrC(0.38f, 0.55f) * nvA * DpTrTunnel(p, rad, pp);
                DpSlAdd(p, rad, ph, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 0, col, al, 0.0f, DPF_SIZE, 1, 20.0f);
            }
            g_dpStretchK = 1.0f;
        }
        if (DP_TR_V8B && age < 1.4f)
            for (j = 0; j < 2 + (int)(R + 0.5f); ++j) {  // v8b: black smoke columns off the nearest nodes, both sides of her path
                const DWORD k = q->id * 5303u + (DWORD)j * 71u + 61u;
                const float side = (j & 1) ? 1.0f : -1.0f, ta = age - 0.08f * FxRnd(k), life = (0.8f + 0.4f * FxRnd(k + 1u)) * (1.0f - 0.5f * R);
                const float rise = DpLerp(DpTrC(150.0f, 45.0f), DpTrC(230.0f, 75.0f), FxRnd(k + 2u)), r0 = DpLerp(DpTrC(18.0f, 14.0f), DpTrC(28.0f, 22.0f), FxRnd(k + 3u));
                const float ph = 6.2831853f * FxRnd(k + 4u);  // (v8c: the smoke stays below her waist: no smoke at her head)
                const float u = FxRnd(k + 6u);
                float f, p[3], al, rad, lat, col[3];
                int c;
                if (ta < 0.0f || ta >= life) continue;
                f = ta / life;
                rad = r0 * (1.0f + DpTrC(1.0f, 0.5f) * f);
                lat = side * (24.0f + 18.0f * FxRnd(k + 5u) + (40.0f + 110.0f * R) * ta) + 6.0f * sinf(1.9f * t + ph);
                p[0] = q->p[0] - q->dir[0] * u * q->seg + px * lat;
                p[1] = q->p[1] - DpTrC(40.0f, 25.0f) - rise * powf(f, 0.75f);
                p[2] = q->p[2] - q->dir[1] * u * q->seg + pz * lat;
                DpTrCarry(q, ta, 0.30f, p);
                al = DpTrC(0.50f, 0.45f) * lvA * DpTrNear(p) * DpTrBig(p, 1.5f * rad) * FxSmooth(0.0f, 0.12f, f) * (1.0f - FxSmooth(0.45f, 1.0f, f));
                g_dpStretchK = DpTrC(1.5f, 1.3f), g_dpStretchD[0] = 0.0f, g_dpStretchD[1] = 1.0f;  // upright
                if (sv) {
                    DpSlAdd(p, rad, ph, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 1, kDpTrSv, 0.06f * al, 0.0f, DPF_SIZE, 1, 0.0f);
                } else {
                    for (c = 0; c < 3; ++c) col[c] = kDpTrAsh[c] * DpLookC();
                    al *= nvA * DpTrTunnel(p, rad, pp);
                    DpSlAdd(p, rad, ph, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 0, col, al > 0.9f ? 0.9f : al, 0.0f, DPF_SIZE, 1, 20.0f);
                }
                g_dpStretchK = 1.0f;
            }
    }
    if (nd[0] || nd[1] || nd[2]) {  // the footprints: under every sprite of the list (they lie on the floor)
        void *tex = FxTexture(dev, FXX_SMOKE);
        int b;
        for (b = 0; b < 3 && tex; ++b)
            if (nd[b]) {
                DpSlStates(dev, tex, b != 1);
                ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(nd[b] / 3), g_dpTrDecV[b], sizeof(SpVtx));
                tris += nd[b] / 3;
            }
    }
    return tris;
}
// ---------------------------------------------------------------- v3.28 TRAIL end
// her violet tendrils: thin broken strips spiralling off the limbs (NOTES_v5 2); Spirit Vision: its colours x 1.2
#define DP_TEND_PTS 38
static void DpHerTendrils(int level)
{
    static const float kCol[3] = { 0.52f, 0.17f, 0.98f }, kUnder[3] = { 0.30f, 0.09f, 0.62f };
    static const float kSvCol[3] = { 0.55f, 0.65f, 1.0f }, kSvUnder[3] = { 0.25f, 0.28f, 0.90f };
    const DpLook *L = &kDpLook[level];
    const float t = g_dpNow, gain = g_dpLook == 3 ? 1.5f : 1.0f;  // v3.27.3: Spirit Vision x 1.5 (v7 1.35; v6 1.6)
    int i;
    if (g_dpLook == 1) return;  // no light in Thermal (v3.27.2: night vision draws them green-white)
    for (i = 0; i < L->tN; ++i) {
        const float life = DpLerp(3.5f, 6.5f, FxRnd((DWORD)i * 4973u + 3u)), tt = t + FxRnd((DWORD)i * 8221u + 5u) * life;
        const int cyc = (int)floorf(tt / life);
        const float age = tt - (float)cyc * life, f = age / life;
        const DWORD k = (DWORD)i * 389u + (DWORD)cyc * 1723u;
        const int ax = (int)(FxRnd(k + 1u) * 13.0f) % 13;
        const float *A = g_dpHer[kDpHerAxis[ax][0]], *B = g_dpHer[kDpHerAxis[ax][1]];
        float d[3], dn[3], e1[3], e2[3], l, P[DP_TEND_PTS][3], W[DP_TEND_PTS], K[DP_TEND_PTS], W3[DP_TEND_PTS], K3[DP_TEND_PTS];
        const float r0 = DpLerp(12.0f, 20.0f, FxRnd(k + 2u)), sgn = FxRnd(k + 4u) > 0.5f ? 1.0f : -1.0f, om = 0.5f + 0.5f * FxRnd(k + 3u);
        const float ph = 6.28f * FxRnd(k + 5u) + sgn * om * age, turns = DpLerp(0.8f, 1.5f, FxRnd(k + 6u));
        const float s0 = 0.15f + 0.7f * FxRnd(k + 7u), H = DpLerp(L->tH0, L->tH1, FxRnd(k + 8u)), nd = DpLerp(2.5f, 4.5f, FxRnd(k + 9u));
        const float dph = FxRnd(k + 10u) + (FxRnd(k + 11u) > 0.5f ? 0.35f * t : -0.35f * t);
        const float env = FxSmooth(0.0f, 0.15f, f) * (1.0f - FxSmooth(0.65f, 1.0f, f));
        const float base = DpLerp(0.8f, 1.3f, FxRnd(k + 12u)) * L->tA * gain * env, grow = 0.4f + 0.6f * FxSmooth(0.0f, 0.45f, f);
        int q, j;
        if (!(base > 0.004f)) continue;
        for (j = 0; j < 3; ++j) d[j] = B[j] - A[j];
        l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (!(l > 1.0f)) continue;
        for (j = 0; j < 3; ++j) dn[j] = d[j] / l;
        if (fabsf(dn[0]) < 0.9f) e1[0] = 0.0f, e1[1] = dn[2], e1[2] = -dn[1];  // dn x (1, 0, 0)
        else e1[0] = dn[1], e1[1] = -dn[0], e1[2] = 0.0f;                     // dn x (0, 0, 1)
        l = sqrtf(e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]);
        for (j = 0; j < 3; ++j) e1[j] /= l;
        e2[0] = dn[1] * e1[2] - dn[2] * e1[1], e2[1] = dn[2] * e1[0] - dn[0] * e1[2], e2[2] = dn[0] * e1[1] - dn[1] * e1[0];
        for (q = 0; q < DP_TEND_PTS; ++q) {
            const float u = (float)q / (float)(DP_TEND_PTS - 1), ug = u * grow;
            const float a = ph + sgn * 6.2831853f * turns * ug;
            const float rr = r0 * (1.0f + 1.4f * FxSmooth(0.2f, 1.0f, ug)) * (1.0f + 0.12f * sinf(3.0f * u + 0.8f * t + (float)i));
            const float rise = H * powf(ug, 1.6f), axial = s0 + 0.25f * ug;
            const float dash = FxSmooth(0.30f, 0.55f, 0.5f + 0.5f * sinf(6.2831853f * (ug * nd + dph)));
            const float bri = 0.55f + 0.45f * sinf(7.0f * ug + 1.3f * t + 2.1f * (float)i);
            const float tip = powf(1.0f - ug, 1.3f) * FxSmooth(0.0f, 0.08f, ug);
            for (j = 0; j < 3; ++j) P[q][j] = A[j] + d[j] * axial + rr * (cosf(a) * e1[j] + sinf(a) * e2[j]);
            P[q][1] -= rise;  // Y down
            W[q] = 0.9f + 0.5f * powf(sinf(3.14159265f * ug), 0.5f) * (1.0f - 0.5f * ug);
            K[q] = base * dash * bri * tip;
            W3[q] = 3.0f * W[q];
            K3[q] = 0.30f * K[q];
        }
        if (g_dpLook != 2) DpRibbon(P, W3, K3, DP_TEND_PTS, g_dpLook == 3 ? kSvUnder : kUnder, 2.2f * g_svUnit);  // the under-strip
        DpRibbon(P, W, K, DP_TEND_PTS, g_dpLook == 3 ? kSvCol : g_dpLook == 2 ? kDpNvCol : kCol, 1.2f * g_svUnit);
    }
}

// ---------------------------------------------------------------- C. victims by their stain s (PLAN 4.3; NOTES_v5 3, v4 C)
#define DP_VICTIM_FULL 6  // review DP-R502: every victim gets its body cue; above this many the detail per victim scales
                          // down (puffs, motes, smoke tendrils; no midpoints) so all of them fit the list
static float g_dpVicDetail = 1.0f;
static const float kDpGlint[3] = { 0.50f, 0.16f, 0.95f }, kDpClingCol[3] = { 0.060f, 0.017f, 0.105f };
static const float kDpRiseCol[3] = { 0.100f, 0.028f, 0.175f }, kDpSvCol[3] = { 0.30f, 0.30f, 0.95f };
static void DpVictimSprites(const DpArt *a, int vi, float floorY)
{
    BYTE *m = DpModelOf(a->task, a->kind);
    const float *mats;
    const BYTE *skel;
    float B[32][3];
    int nb = 0, q, n, seg[32][2], ns = 0;
    const float s = a->r, t = g_dpNow;
    const float nvC = DpLookC(), nvA = g_dpLook == 2 ? 1.3f : 1.0f;
    if (!m) return;
    mats = *(const float *const *)(m + 0xC0);
    skel = *(const BYTE *const *)(m + 0xE0);
    if (!mats || !skel) return;
    nb = *(const int *)(skel + 0x14);
    if (nb <= 0) return;
    if (nb > 32) nb = 32;
    for (q = 0; q < nb; ++q) {
        memcpy(B[q], mats + 16 * q + 12, 12);
        if (!DpFinite3(B[q]) || fabsf(B[q][0] - a->pos[0]) > 400.0f || fabsf(B[q][2] - a->pos[2]) > 400.0f) return;  // not this frame's
    }
    floorY = B[0][1];  // review DP3271-IR-003: the halo clamp uses this body's own floor (its lowest bone; Y down), not hers
    for (q = 1; q < nb; ++q) floorY = B[q][1] > floorY ? B[q][1] : floorY;
    for (q = 0; q + 1 < nb && ns < 32; ++q) {  // segments: neighbouring bones 2-70 u apart (as v3.27.0's outline)
        const float dx = B[q + 1][0] - B[q][0], dy = B[q + 1][1] - B[q][1], dz = B[q + 1][2] - B[q][2], l2 = dx * dx + dy * dy + dz * dz;
        if (l2 <= 70.0f * 70.0f && l2 >= 4.0f) seg[ns][0] = q, seg[ns++][1] = q + 1;
    }
    if (g_dpLook == 3) {  // Spirit Vision: a violet-blue spirit glow in proportion to s, glints x 1.5
        for (q = 0; q < 12; ++q) {
            const int bi = q * nb / 12;
            const float w = 0.5f + 0.5f * sinf(t * 1.3f + 1.7f * (float)q + (float)vi);
            DpSlAdd(B[bi], 22.0f + 10.0f * s, 0.7f * (float)q, DPT_SMOKE, DPS_SOFT, 1, kDpSvCol, 0.15f * s * (0.6f + 0.4f * w), 0.0f, 0, 1, 0.0f);
        }
    } else {
        // the dark smoke clinging to its bones
        n = (int)((12.0f + 22.0f * s) * g_dpVicDetail + 0.5f);
        if (n < 4) n = 4;
        for (q = 0; q < n; ++q) {
            const int bi = (q * 5 + vi) % nb;
            const DWORD k = (DWORD)vi * 977u + (DWORD)q * 31u + (DWORD)(ULONG_PTR)a->task;
            const float ph = t * (0.35f + 0.3f * FxRnd(k + 1u)) + 6.2831853f * FxRnd(k + 2u);
            const float rise = 16.0f * fmodf(t * 0.25f + FxRnd(k + 3u), 1.0f);
            float p[3], al, c[3];
            int j;
            p[0] = B[bi][0] + 11.0f * cosf(ph);
            p[1] = B[bi][1] - rise - 4.0f * sinf(ph * 0.7f);
            p[2] = B[bi][2] + 11.0f * sinf(ph);
            al = 0.40f * (0.25f + 0.95f * s) * (0.75f + 0.25f * sinf(t * 1.7f + (float)(k & 255u))) * nvA;
            for (j = 0; j < 3; ++j) c[j] = kDpClingCol[j] * nvC * DP_VOL_BRIGHT;
            DpSlAdd(p, 20.0f + 22.0f * s, FxRnd(k + 5u) * 6.2831853f, DPT_SMOKE, DPS_SMOKE0 + (int)(k & 1u), 0, c, al < 0.6f ? al : 0.6f, 0.0f, 0, 1, 0.0f);
        }
        // the black halo on its bones (12) and segment midpoints, pushed behind them
        for (q = 0; q < 12 && q < nb; ++q) {
            const int bi = q * nb / (nb < 12 ? nb : 12);
            const float al = 0.75f * (0.2f + 0.8f * s) * (0.85f + 0.15f * sinf(0.9f * t + 1.3f * (float)bi + (float)vi)) * nvA;
            float p[3];
            DpAway(B[bi], DpLerp(18.0f, 25.0f, FxRnd((DWORD)bi * 17u + (DWORD)vi)), p);
            DpHaloSprite(p, 22.0f * (0.8f + 0.3f * s) * DP_HALO_RMUL, 0.5f * (float)bi, al, 0.35f * al, floorY);
        }
        for (q = 0; g_dpVicDetail >= 1.0f && q < ns; ++q) {
            float mid[3], p[3];
            int j;
            const float al = 0.75f * 0.9f * (0.2f + 0.8f * s) * (0.85f + 0.15f * sinf(0.9f * t + 1.1f * (float)q + (float)vi)) * nvA;
            for (j = 0; j < 3; ++j) mid[j] = 0.5f * (B[seg[q][0]][j] + B[seg[q][1]][j]);
            DpAway(mid, DpLerp(18.0f, 25.0f, FxRnd((DWORD)q * 29u + (DWORD)vi)), p);
            DpHaloSprite(p, (10.0f + 1.4f * 8.0f) * (0.8f + 0.3f * s), 0.5f * (float)q, al, 0.0f, floorY);
        }
    }
    if (g_dpLook == 1) return;  // no light in Thermal (night vision: green-white glints, v3.27.2)
    for (q = 0; q < ns; ++q) {  // violet glints sliding along the bone segments (the outline)
        int e;
        for (e = 0; e < 2; ++e) {
            const DWORD k = (DWORD)vi * 613u + (DWORD)q * 17u + (DWORD)e;
            const float u = fmodf(t * 0.35f + FxRnd(k), 1.0f);
            float p[3];
            int j;
            for (j = 0; j < 3; ++j) p[j] = B[seg[q][0]][j] * (1.0f - u) + B[seg[q][1]][j] * u;
            if (g_dpLook == 2) {  // v7 (review DP3272-ART-002): night vision: a green-white filament 22 u along the segment,
                // lifted 9 u toward the camera (the body's depth would hide it), strength max(0.08 + 0.92 s^2, 0.35) x 1.6 x
                // sin(pi u) (the stain fade: g_dpAlphaK in DpSlAdd), plus a 3x wide glow at 0.30; one per segment. Review
                // DP3272-R3-001: a sprite stretched along the segment on screen, in the sprite list (2048, by priority), not
                // a ribbon, so no victim loses its filaments to the frame's 128-ribbon budget
                float d[3], dl, str, q1[3], h0[4], h1[4], x0, y0, x1, y1;
                if (e) continue;
                for (j = 0; j < 3; ++j) d[j] = B[seg[q][1]][j] - B[seg[q][0]][j];
                dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                if (!(dl > 1e-3f) || !g_svVp) continue;
                str = 0.08f + 0.92f * s * s;
                if (str < 0.35f) str = 0.35f;
                str *= 1.6f * sinf(3.14159265f * u);
                DpAway(p, -9.0f, p);
                for (j = 0; j < 3; ++j) q1[j] = p[j] + d[j] / dl * 11.0f;
                GvXform(p, h0), GvXform(q1, h1);
                g_dpStretchK = 1.0f;
                if (GvScreen(h0, g_svVp, &x0, &y0) && GvScreen(h1, g_svVp, &x1, &y1)) {
                    // the direction in the sprite's rx / ry space, and the half length (11 u x its on-screen share) / radius
                    const float ux = (x1 - x0) / g_svPxX * h0[3], uy = (y1 - y0) / g_svPxY * h0[3], ul = sqrtf(ux * ux + uy * uy);
                    if (ul > 1e-3f && _finite(ul)) {
                        g_dpStretchD[0] = ux / ul, g_dpStretchD[1] = uy / ul;
                        g_dpStretchK = ul / 2.2f > 1.0f ? ul / 2.2f : 1.0f;
                    }
                }
                DpSlAdd(p, 2.2f, 0.0f, DPT_ATLAS, SVC_SOFT, 1, kDpNvCol, str, 1.0f * g_svUnit, 0, 2, 0.0f);
                g_dpStretchK = g_dpStretchK / 3.0f > 1.0f ? g_dpStretchK / 3.0f : 1.0f;  // the glow: 3x wide (6.6 u), as long (ul / 6.6)
                DpSlAdd(p, 6.6f, 0.0f, DPT_ATLAS, SVC_SOFT, 1, kDpNvCol, 0.30f * str, 2.0f * g_svUnit, 0, 1, 0.0f);
                g_dpStretchK = 1.0f;
                continue;
            }
            DpSlAdd(p, 4.5f + 4.0f * s, 0.0f, DPT_ATLAS, SVC_SOFT, 1, g_dpLook == 3 ? kDpSvCol : g_dpLook == 2 ? kDpNvCol : kDpGlint,
                    (g_dpLook == 3 ? 1.5f : 1.0f) * 0.45f * (0.08f + 0.92f * s * s) * sinf(3.14159265f * u), 1.5f * g_svUnit, 0, 1, 0.0f);
        }
    }
    if (g_dpLook == 3 || g_dpLook == 2) return;  // (no motes in Spirit Vision / night vision)
    if (s > 0.3f) {  // motes
        const int nm = (int)((6.0f + 18.0f * (s - 0.3f) / 0.7f) * g_dpVicDetail + 0.5f);
        for (q = 0; q < nm; ++q) {
            const DWORD k = (DWORD)vi * 6151u + (DWORD)q * 53u;
            const float life = 1.4f + 0.8f * FxRnd(k + 1u), tt = t + FxRnd(k + 2u) * life;
            const int cy = (int)floorf(tt / life);
            const float age = tt - (float)cy * life, f = age / life;
            const DWORD kk = k + (DWORD)cy * 977u;
            const int bi = (int)(FxRnd(kk + 3u) * (float)nb) % nb;
            float p[3];
            p[0] = B[bi][0] + 8.0f * sinf(age * 2.0f + (float)(kk & 255u));
            p[1] = B[bi][1] - 50.0f * age;
            p[2] = B[bi][2] + 8.0f * cosf(age * 2.3f + (float)(kk & 255u));
            DpSlAdd(p, 4.0f + 3.0f * FxRnd(kk + 4u), 0.0f, DPT_ATLAS, SVC_ORB, 1, kDpMoteCol,
                    0.55f * FxSmooth(0.0f, 0.15f, f) * (1.0f - FxSmooth(0.4f, 1.0f, f)) * (s - 0.1f), 1.5f * g_svUnit, 0, 1, 0.0f);
        }
    }
    if (s > 0.5f) {  // rising smoke tendrils from its highest bones (smallest y: Y down)
        const int nt = (int)((3.0f + 8.0f * (s - 0.5f) / 0.5f) * g_dpVicDetail + 0.5f);
        int top[3] = { 0, 0, 0 }, j;
        for (j = 0; j < nb; ++j) {  // the three highest bones
            if (B[j][1] < B[top[0]][1]) top[2] = top[1], top[1] = top[0], top[0] = j;
            else if (B[j][1] < B[top[1]][1]) top[2] = top[1], top[1] = j;
            else if (B[j][1] < B[top[2]][1]) top[2] = j;
        }
        for (q = 0; q < nt; ++q) {
            const DWORD k = (DWORD)vi * 7919u + (DWORD)q * 101u;
            const float life = 2.4f + FxRnd(k + 1u), tt = t + FxRnd(k + 2u) * life;
            const int cy = (int)floorf(tt / life);
            const float age = tt - (float)cy * life, f = age / life, env = FxSmooth(0.0f, 0.2f, f) * (1.0f - FxSmooth(0.6f, 1.0f, f));
            const DWORD kk = k + (DWORD)cy * 389u;
            const float *b0 = B[top[(int)(FxRnd(kk + 3u) * 3.0f) % 3]];
            int v;
            for (v = 0; v < 7; ++v) {
                const float vv = (float)v / 6.0f;
                float p[3];
                p[0] = b0[0] + 6.0f * sinf(3.0f * vv + 1.7f * t + (float)(kk & 63u));
                p[1] = b0[1] - 70.0f * vv * (0.4f + 0.6f * f);
                p[2] = b0[2] + 6.0f * cosf(2.6f * vv + 1.3f * t + (float)(kk & 63u));
                DpSlAdd(p, 7.0f + 6.0f * vv, 0.9f * (float)v, DPT_SMOKE, DPS_SMOKE1, 0, kDpRiseCol, 0.22f * s * env * (1.0f - 0.5f * vv), 0.0f, 0, 1, 0.0f);
            }
        }
    }
}

// the raised wisps (PLAN 8.3 v3): 48 vertical strands round her lower body, FXX_WISP billboards, premultiplied
static const float kDpWispCol[3][3] = { { 0.110f, 0.030f, 0.190f }, { 0.060f, 0.050f, 0.210f }, { 0.170f, 0.030f, 0.170f } };
typedef struct { float x, y, z, rhw, hx, hy, depth, u0; DWORD c; } DpWispQ;
static int DpWispCmp(const void *a, const void *b)
{
    const float x = ((const DpWispQ *)a)->depth, y = ((const DpWispQ *)b)->depth;
    return x < y ? 1 : x > y ? -1 : 0;
}
static int DpWisps(void *dev, const Viewport8 *vp, float R, int nW)
{
    static DpWispQ Q[2 * DP_WISP_N];
    static SpVtx v[6 * 2 * DP_WISP_N];
    void *tex;
    int i, nq = 0, n = 0;
    for (i = 0; i < nW && i < DP_WISP_N; ++i) {
        const float life = 4.0f + 3.0f * FxRnd((DWORD)i * 6007u + 5u), tt = g_dpNow + FxRnd((DWORD)i * 7717u + 9u) * life;
        const int cyc = (int)floorf(tt / life);
        const float age = tt - (float)cyc * life, f = age / life;
        const DWORD k = (DWORD)i * 197u + (DWORD)cyc * 1031u;
        const float rr = R * (0.15f + 0.45f * FxRnd(k + 1u));
        const float th = 6.2831853f * FxRnd(k + 2u) + ((i & 1) ? 1.0f : -1.0f) * (0.12f + 0.14f * FxRnd(k + 3u)) * age;
        const float hh = 40.0f + 22.0f * FxRnd(k + 4u), hw = 13.0f + 9.0f * FxRnd(k + 5u);  // v3.27.1: half height 40-62
        const float bottom = 20.0f + 16.0f * FxRnd(k + 6u) + 7.0f * age;
        const float env = FxSmooth(0.0f, 0.25f, f) * (1.0f - FxSmooth(0.6f, 1.0f, f));
        const float *col = kDpWispCol[(int)(FxRnd(k + 7u) * 3.0f) % 3];
        float p[3], h[4], x, y, a, d;
        DpWispQ *q;
        if (g_dpWisp[i].cycle != cyc) {
            g_dpWisp[i].cycle = cyc;
            memcpy(g_dpWisp[i].anchor, g_dpCen, sizeof g_dpWisp[i].anchor);
        }
        p[0] = g_dpWisp[i].anchor[0] + rr * cosf(th);
        p[1] = g_dpWisp[i].anchor[1] - (bottom + hh);  // the centre (Y down)
        p[2] = g_dpWisp[i].anchor[2] + rr * sinf(th);
        a = 0.34f * env * (nW < DP_WISP_N ? 0.6f : 1.0f) * (g_dpLook == 2 ? 1.3f : 1.0f);  // v3.27.1: Weak 0.6; night vision
        if (g_svEyeOk) {
            const float ex = p[0] - g_svEye[0], ey = p[1] - g_svEye[1], ez = p[2] - g_svEye[2];
            d = sqrtf(ex * ex + ey * ey + ez * ez);
            a *= FxSmooth(DP_NEAR0, DP_NEAR1, d - hh);  // v3.27.1: the DP near fade (PLAN 2.2)
        }
        if (!(a > 0.004f)) continue;
        GvXform(p, h);
        if (!GvScreen(h, vp, &x, &y)) continue;
        q = &Q[nq++];
        q->x = x, q->y = y, q->z = SvZH(h), q->rhw = 1.0f / h[3], q->depth = h[3];
        q->hx = 2.0f * hw * g_svPxX / h[3];
        q->hy = hh * g_svPxY / h[3];
        {   // review DP-R404: the curl travels: the strand cells are curl phases 90 degrees apart; the reference's phase
            // runs 0.6 rad / s, so the wisp cross-fades into the next cell every 2.6 s (two quads, weights 1 - f / f)
            const float ph = 0.6f * g_dpNow / 1.5708f + 4.0f * FxRnd(k + 8u), fl = floorf(ph), fw = ph - fl;
            const int c0 = (int)fl & 3;
            int e;
            for (e = 0; e < 2 && nq < 2 * DP_WISP_N; ++e) {
                const float ae = a * (e ? fw : 1.0f - fw), cm = DpLookC();
                DpWispQ *qq = e ? &Q[nq++] : q;
                if (e) *qq = *q;
                qq->u0 = 0.25f * (float)((c0 + e) & 3);
                qq->c = (DWORD)(FxSat(ae) * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(col[0] * cm * ae) * 255.0f + 0.5f) << 16 |
                        (DWORD)(FxSat(col[1] * cm * ae) * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(col[2] * cm * ae) * 255.0f + 0.5f);
            }
        }
    }
    if (!nq || (tex = FxTexture(dev, FXX_WISP)) == NULL) return 0;
    qsort(Q, (size_t)nq, sizeof Q[0], DpWispCmp);
    for (i = 0; i < nq; ++i) {
        static const float kC[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };  // screen y down: -1 = top
        static const int kT[6] = { 0, 1, 2, 1, 3, 2 };
        int m;
        for (m = 0; m < 6; ++m) {
            const int c = kT[m];
            SpVtx *w = &v[n++];
            w->x = Q[i].x + kC[c][0] * Q[i].hx - 0.5f;
            w->y = Q[i].y + kC[c][1] * Q[i].hy - 0.5f;
            w->z = Q[i].z;
            w->rhw = Q[i].rhw;
            w->c = Q[i].c;
            w->u = Q[i].u0 + (kC[c][0] > 0 ? 0.25f - 0.5f / 256.0f : 0.5f / 256.0f);
            w->v = kC[c][1] > 0 ? 1.0f - 0.5f / 128.0f : 0.5f / 128.0f;
        }
    }
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    FxSampler(dev, tex, 0, 0);  // clamp, linear
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
    return n / 3;
}


// Room 302: a dark violet column at each haunting being eaten (PLAN 8.6 of v3.27.0)
static void DpHauntColumns(void)
{
    int i, q;
    if (!g_dpInR302 || g_dpLook == 3) return;  // review DP-I301
    for (i = 0; i < 15; ++i) {
        const float *c = g_dpHauntCol[i];
        if (!(c[3] > 0.0f)) continue;
        for (q = 0; q < 6; ++q) {
            const DWORD k = (DWORD)i * 31u + (DWORD)q * 7u;
            const float life = 2.0f + FxRnd(k), tt = g_dpNow + FxRnd(k + 1u) * life, f = fmodf(tt, life) / life;
            float p[3];
            p[0] = c[0] + 20.0f * sinf(g_dpNow * 0.7f + (float)q);
            p[1] = c[1] - 160.0f * f;
            p[2] = c[2] + 20.0f * cosf(g_dpNow * 0.6f + (float)q);
            const float lc = DpLookC(), cc[3] = { kDpVPuffCol[0] * lc, kDpVPuffCol[1] * lc, kDpVPuffCol[2] * lc };  // DP3272-004
            DpSlAdd(p, 50.0f + 30.0f * f, FxRnd(k + 2u) * 6.28f, DPT_ATLAS, SVC_MIST0 + (int)(q & 1), 0, cc,
                    0.35f * FxSmooth(0.0f, 0.2f, f) * (1.0f - FxSmooth(0.6f, 1.0f, f)) * (g_dpLook == 2 ? 1.3f : 1.0f), 0.0f, 0, 1, 0.0f);
        }
    }
}

// Spirit Vision veins (NOTES_v5 5): 40 random walks of 30 points as continuous tapered strips, anchored where the fog's
// centre was at each vein's birth
#define DP_VEIN_N   40
#define DP_VEIN_PTS 30
typedef struct { float anchor[3]; int cycle; } DpVeinSlot;
static DpVeinSlot g_dpVein[DP_VEIN_N];
static void DpSvVeins(float R)
{
    static const float kVein[3] = { 0.55f, 0.65f, 1.0f }, kVeinUnder[3] = { 0.25f, 0.28f, 0.90f };
    const float t = g_dpNow;
    int i;
    for (i = 0; i < DP_VEIN_N; ++i) {
        const float life = DpLerp(3.0f, 6.0f, FxRnd((DWORD)i * 3571u + 1u)), tt = t + FxRnd((DWORD)i * 7127u + 2u) * life;
        const int n = (int)floorf(tt / life);
        const float age = tt - (float)n * life, f = age / life;
        const DWORD k = (DWORD)i * 211u + (DWORD)n * 1511u;
        const float rr = R * 0.8f * sqrtf(FxRnd(k + 1u)), th = 6.2831853f * FxRnd(k + 2u);
        const float env = FxSmooth(0.0f, 0.2f, f) * (1.0f - FxSmooth(0.6f, 1.0f, f));
        float P[DP_VEIN_PTS][3], W[DP_VEIN_PTS], K[DP_VEIN_PTS], W3[DP_VEIN_PTS], K3[DP_VEIN_PTS], p[3], d[3], l, hgt;
        int q, j;
        if (g_dpVein[i].cycle != n) {
            g_dpVein[i].cycle = n;
            memcpy(g_dpVein[i].anchor, g_dpCen, sizeof g_dpVein[i].anchor);
        }
        if (!(env > 0.004f)) continue;
        hgt = DpLerp(15.0f, 90.0f, FxRnd(k + 3u));  // height above the floor (the walk is in a y-up frame, then Y down)
        p[0] = rr * cosf(th), p[1] = hgt, p[2] = rr * sinf(th);
        d[0] = FxRnd(k + 4u) - 0.5f, d[1] = 0.35f * (FxRnd(k + 5u) - 0.3f), d[2] = FxRnd(k + 6u) - 0.5f;
        l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (j = 0; j < 3; ++j) d[j] /= l > 1e-4f ? l : 1.0f;
        for (q = 0; q < DP_VEIN_PTS; ++q) {
            const float u = (float)q / (float)(DP_VEIN_PTS - 1), pulse = 0.65f + 0.35f * sinf(0.9f * (float)q - 4.0f * t + (float)i);
            P[q][0] = g_dpVein[i].anchor[0] + p[0];
            P[q][1] = g_dpVein[i].anchor[1] - p[1];
            P[q][2] = g_dpVein[i].anchor[2] + p[2];
            W[q] = 0.55f * powf(sinf(3.14159265f * (0.05f + 0.95f * u)), 0.7f);
            K[q] = 0.70f * env * pulse * sqrtf(1.0f - u);  // v3.27.2: 0.70 (v7; v1 0.55, v6 0.80)
            W3[q] = 3.2f * W[q];
            K3[q] = 0.22f * K[q];
            d[0] += 0.30f * (FxRnd(k + 10u + (DWORD)q) - 0.5f);
            d[1] += 0.30f * 0.5f * (FxRnd(k + 40u + (DWORD)q) - 0.5f);
            d[2] += 0.30f * (FxRnd(k + 70u + (DWORD)q) - 0.5f);
            l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            for (j = 0; j < 3; ++j) d[j] /= l > 1e-4f ? l : 1.0f;
            for (j = 0; j < 3; ++j) p[j] += d[j] * 7.0f;
            if (p[1] < 8.0f) p[1] = 8.0f, d[1] = fabsf(d[1]);
        }
        DpRibbon(P, W3, K3, DP_VEIN_PTS, kVeinUnder, 1.8f * g_svUnit);
        DpRibbon(P, W, K, DP_VEIN_PTS, kVein, 1.0f * g_svUnit);
    }
}

// ---------------------------------------------------------------- the sprite part of a frame
static int g_dpVictimsDrawn;
static int DpSprFrame(void *dev, const Viewport8 *vp, float R, int level, const float pp[3])
{
    int i, nv = 0, tris = 0;
    g_svVp = vp;
    if (!SvSprScale(vp)) return 0;
    g_svUnit = (float)vp->h / 720.0f;
    g_dpSLN = 0;
    g_dpRibN = g_dpRibs = 0;
    g_dpFloorY = g_dpCen[1];
    if (g_dpGovStamp != g_bgFrameNo) {  // v3.35 FX335: the budget's share, once a frame (the scene pass or the post-filter one)
        g_dpGovStamp = g_bgFrameNo;
        g_dpGovK = g_bgOn ? BgRamp(g_dpGovK, BgDpKeep(BgLevel()), g_bgDt, 0.75f) : 1.0f;
    }
    DpBasis(vp, pp);  // the camera's world right / down for the floor clip (review DP3271-IR-002)
    DpHerVelocity(pp);  // v3.27.2: the hem trails behind her
    DpHerBones(pp);
    DpTrailStep(pp, g_dpHerOk);  // v3.28 TRAIL: her path's footprints (once a frame) and the black fire on them (the
    tris += DpHerTrail(dev, level, pp);  // footprints drawn now: on the pool, under the wisps and the sprite list)
    DpVolume(R, level, pp);
    DpHauntColumns();
    if (g_dpLook == 3) {
        static const float kSv[3] = { 0.30f, 0.30f, 0.95f };
        DpSvVeins(R);
        if (g_dpHerOk) {  // a spirit glow round her torso
            static const struct { int b; float r; } kG[3] = { { HB_CHEST, 40.0f }, { HB_PELVIS, 36.0f }, { HB_HEAD, 26.0f } };
            for (i = 0; i < 3; ++i) {
                float p[3];
                DpAway(g_dpHer[kG[i].b], 10.0f, p);
                DpSlAdd(p, kG[i].r, 1.0f, DPT_SMOKE, DPS_SOFT, 1, kSv, 0.16f, 0.0f, 0, 2, 0.0f);  // v3.27.2: 0.16
            }
        }
    } else {
        tris += DpWisps(dev, vp, R, kDpLook[level].wisps);  // the raised wisps (their own draw, premultiplied)
        if (g_dpHerOk) DpHerHalo(level, g_dpFloorY);
    }
    if (g_dpHerOk) DpHerTendrils(level);
    for (i = 0; i < g_dpArtN; ++i) nv += g_dpArt[i].r > DP_STAIN_MIN;
    g_dpVicDetail = nv > DP_VICTIM_FULL ? (float)DP_VICTIM_FULL / (float)nv : 1.0f;
    for (i = 0, nv = 0; i < g_dpArtN; ++i)
        if (g_dpArt[i].r > DP_STAIN_MIN) {  // review DP3271-001: every cue goes to 0 at the end of the stain (no pop)
            // v3.28 (user v3.27.3 item 5): the wrap fades in slowly: x smoothstep(0.02, 0.40, s) on top of the stain fade
            // (full at s 0.40 = 4 s inside, was 0.15 = 1.5 s: the 0.2 base of the halo alpha read as a pop)
            g_dpAlphaK = DpStainFade(g_dpArt[i].r) * FxSmooth(DP_STAIN_MIN, 0.40f, g_dpArt[i].r);
            DpVictimSprites(&g_dpArt[i], nv++, 0.0f);  // (its floor: its own lowest bone, review DP3271-IR-003)
            g_dpAlphaK = 1.0f;
        }
    g_dpVictimsDrawn = nv;
    tris += 2 * DpSlDraw(dev);
    tris += DpRibDraw(dev);
    return tris;
}

// v3.27.2 (PLAN 2.4): the hidden depth probe ([ScreenEffects] DeathPresenceProbe=1): 9 squares on the floor 120 u from
// her toward the camera (beyond her when the camera is closer than 240 u), square q at depth x 2^((q - 4) / 2) (x 0.25 .. x 4), red .. white; a screenshot shows
// which depth scales pass the floor's depth test
// v3.27.3 (re_depth.md 6): the depth contour probe ([ScreenEffects] DeathPresenceProbe=2; 3 = the same with v3.27.2's
// x 1/128): 12 full-screen quads at the exact stored depth of w_k = 100 x 1.35^k (100 .. 2700 u), nearest first, opaque,
// GREATEREQUAL, no z write: each pixel ends with the colour of the farthest w_k still in front of the stored depth, a
// terrace map of the depth buffer in view-depth units (pixels nearer than 100 u keep the scene)
static int DpContour(void *dev, const Viewport8 *vp, int old)
{
    static const DWORD kCol[12] = { 0xFF0000u, 0xFF7F00u, 0xFFFF00u, 0x7FFF00u, 0x00FF00u, 0x00FF9Fu, 0x00FFFFu, 0x007FFFu,
                                    0x0000FFu, 0x7F00FFu, 0xFF00FFu, 0xFFFFFFu };
    static int s_logged;
    SpVtx v[12 * 6];
    int k, m, n = 0;
    if (!g_svPOk || !vp) return 0;
    for (k = 0; k < 12; ++k) {
        const float w = 100.0f * powf(1.35f, (float)k), d = w * g_svP[11] + g_svP[15];
        float z = d != 0.0f ? (w * g_svP[10] + g_svP[14]) / d : 0.0f;
        static const int kT[6] = { 0, 1, 3, 0, 3, 2 };
        SpVtx c4[4];
        if (old) z *= SV_ZSCALE;
        z = z < 0.0f ? 0.0f : z > 1.0f ? 1.0f : z;
        for (m = 0; m < 4; ++m) {
            c4[m].x = (float)vp->x + ((m & 1) ? (float)vp->w : 0.0f) - 0.5f;
            c4[m].y = (float)vp->y + ((m & 2) ? (float)vp->h : 0.0f) - 0.5f;
            c4[m].z = z, c4[m].rhw = 1.0f / w;
            c4[m].c = 0xFF000000u | kCol[k];
            c4[m].u = c4[m].v = 0.0f;
        }
        for (m = 0; m < 6; ++m) v[n++] = c4[kT[m]];
    }
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 3 /*SELECTARG2*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 3 /*SELECTARG2*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
    if (!s_logged++)
        Log("death presence: the depth contour probe draws (%s depth): stored depth nearer than 100 u = the scene; then red 100, "
            "orange 135, yellow 182, lime 246, green 332, mint 448, cyan 605, sky 817, blue 1103, violet 1489, magenta 2010, "
            "white 2714 u and farther / cleared", old ? "x 1/128" : "exact");
    return n / 3;
}
static int DpProbe(void *dev, const Viewport8 *vp, const float pp[3])
{
    if (g_dpProbe >= 2) return DpContour(dev, vp, g_dpProbe == 3);   // review DP3272-005: world-space floor quads (32 x 32 u, 1 u above her floor), each vertex projected, its depth x the
    // square's scale: the same geometry as SpiritTest 4; untextured additive (no light suppression in any look)
    static const float kCol[9][3] = { { 1, 0, 0 }, { 1, 0.5f, 0 }, { 1, 1, 0 }, { 0.5f, 1, 0 }, { 0, 1, 0 },
                                      { 0, 1, 1 }, { 0, 0.3f, 1 }, { 0.7f, 0, 1 }, { 1, 1, 1 } };
    static int s_logged, s_place = -1;
    SpVtx v[9 * 6];
    float dx, dz, l, d;
    int q, n = 0, place, drop = 0;
    if (!g_svEyeOk) return 0;
    dx = g_svEye[0] - pp[0], dz = g_svEye[2] - pp[2], l = sqrtf(dx * dx + dz * dz);
    if (!(l > 1.0f)) dx = 0.0f, dz = 1.0f, l = 1.0f;
    dx /= l, dz /= l;
    // review DP3272-R2-001: the row lies between her and the camera only when the camera is far enough (horizontal
    // distance >= 240 u: the row at 120 u, half way or nearer); a closer camera would put the row at or behind its near
    // plane, so the row goes 120 u beyond her (away from the camera), where a close camera sees the floor round her
    place = l >= 240.0f;
    d = place ? 120.0f : -120.0f;
    for (q = 0; q < 9; ++q) {
        const float mul = powf(2.0f, (float)(q - 4) / 2.0f);
        const float cx = pp[0] + dx * d + (-dz) * (40.0f * (float)q - 160.0f), cz = pp[2] + dz * d + dx * (40.0f * (float)q - 160.0f);
        static const int kT[6] = { 0, 1, 3, 0, 3, 2 };
        SpVtx c4[4];
        int m, ok = 1;
        for (m = 0; m < 4 && ok; ++m) {
            const float w[3] = { cx + ((m & 1) ? 16.0f : -16.0f), pp[1] - 1.0f, cz + ((m & 2) ? 16.0f : -16.0f) };
            float h[4], x, y;
            GvXform(w, h);
            if (!GvScreen(h, vp, &x, &y)) ok = 0;
            c4[m].x = x - 0.5f, c4[m].y = y - 0.5f, c4[m].z = SvZH(h) * mul, c4[m].rhw = 1.0f / h[3];
            c4[m].c = 0xFF000000u | (DWORD)(kCol[q][0] * 255.0f) << 16 | (DWORD)(kCol[q][1] * 255.0f) << 8 | (DWORD)(kCol[q][2] * 255.0f);
            c4[m].u = c4[m].v = 0.0f;
        }
        if (!ok) { ++drop; continue; }
        for (m = 0; m < 6; ++m) v[n++] = c4[kT[m]];
    }
    if (place != s_place) {  // once per placement change: where the row went and how many squares left the screen
        s_place = place;
        Log("death presence: the depth probe row lies %s (camera %.0f u from her horizontally), %d of 9 squares off screen",
            place ? "120 u toward the camera" : "120 u beyond her (the camera is close)", l, drop);
    }
    if (!n) return 0;
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 3 /*SELECTARG2*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 3 /*SELECTARG2*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 0);
    ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), v, sizeof(SpVtx));
    if (!s_logged++) Log("death presence: the depth probe draws (9 floor squares between her and the camera, left to right depth x "
                         "0.25 red, x 0.35 orange, x 0.5 yellow, x 0.71 lime, x 1 green, x 1.41 cyan, x 2 blue, x 2.83 violet, x 4 white)");
    return n / 3;
}
static void DpSprReset(void)  // a new room: every hashed slot re-anchors at its next life
{
    int i;
    for (i = 0; i < DP_FRAY_N; ++i) g_dpFray[i].cycle = -1;
    g_dpVelOk = 0;
    DpTrailReset();  // v3.28 TRAIL
    for (i = 0; i < DP_WISP_N; ++i) g_dpWisp[i].cycle = -1;
    for (i = 0; i < DP_VEIN_N; ++i) g_dpVein[i].cycle = -1;
    DpVolReset();
}
