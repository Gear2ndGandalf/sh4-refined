// ================================================================ v3.27.0 Death Presence: the fog (PLAN section 8)
// Drawn from the scene pass (items.inc SparkleFrame at 0x4EFBFD) before Item Shine and the scene filter, so the filters
// colour it like the world. Python reference for the look: tools/fog_preview.py (renders: staging/v327_death_presence/
// renders). Game Y is down: a layer h units above her feet is at feet.y - h. Every motion is in real seconds (a clock
// that advances only when the previous game frame also drew: KB 138), so the look does not depend on the frame rate.
#define DP_RINGS     12
#define DP_SECTS     32
#define DP_NLAYER    2         // v3.27.1: the floor pool only (4 u, 30 u); the 70 / 110 u layers read as lines from the OTS camera
#define DP_WALLS     32
#define DP_MASK_IN   0.55f
#define DP_MASK_OUT  1.15f
#define DP_RIM_AMP   0.10f
#define DP_TEX_LO    0.12f
#define DP_TEX_HI    0.65f
#define DP_HI_LO     0.62f
#define DP_HI_HI     0.92f
#define DP_HI_GAIN   0.15f
#define DP_WALL_LEN  40.0f
#define DP_WALL_PRE  40.0f     // the wall fade starts this far before the hit
#define DP_LAG_S     0.35f     // the fog's centre follows her with this lag
#define DP_TEX_N     128
#define DP_NEAR0     20.0f     // v3.27.1 (PLAN 2.2): the fog's near-camera fade 20 -> 90 u (was SV_NEAR0 / NEAR1 60 / 250)
#define DP_NEAR1     90.0f

typedef struct { float h, dens, scale, sx, sz, rot; int hi; } DpLayerK;
static const DpLayerK kDpLayer[DP_NLAYER] = {  // (dens: v3.27.0's; v3.27.1 draws with the level's pool densities, kDpLook)
    { 4.0f, 0.85f, 380.0f, 14.0f, 6.0f, 0.045f, 0 },
    { 30.0f, 0.35f, 260.0f, -9.0f, 12.0f, -0.060f, 1 },
};
static const float kDpBody[3] = { 0.018f, 0.006f, 0.030f };
static const float kDpHiCol[3] = { 0.16f, 0.05f, 0.30f };
static const float *g_dpHiRgb = kDpHiCol;  // the additive disc pass: colour and gain (Spirit Vision: its own, PLAN 5.1)
static float g_dpHiGain = DP_HI_GAIN;
static int g_dpHoleOn, g_dpDiscLook;       // v3.27.1: a hole in the main discs round her shins (NOTES_v5 1); the look
static float g_dpHoleC[3];

// ---------------------------------------------------------------- FXX_FOG: a tileable 4-octave fBm with a domain warp
static BYTE g_dpTexA[DP_TEX_N * DP_TEX_N], g_dpTexC[DP_TEX_N * DP_TEX_N], g_dpTexN[DP_TEX_N * DP_TEX_N];  // coverage, crest, raw
static int g_dpTexBuilt;
static float DpLat(int x, int y, int per, DWORD seed) { return FxRnd(seed ^ (DWORD)(((x % per + per) % per) * 7919 + ((y % per + per) % per) * 104729)); }
static float DpPNoise(float u, float v, int per, DWORD seed)  // periodic value noise, period 1 in u / v
{
    const float x = u * (float)per, y = v * (float)per, fx0 = floorf(x), fy0 = floorf(y);
    const int xi = (int)fx0, yi = (int)fy0;
    const float fx = x - fx0, fy = y - fy0, sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
    const float a = DpLat(xi, yi, per, seed) + (DpLat(xi + 1, yi, per, seed) - DpLat(xi, yi, per, seed)) * sx;
    const float b = DpLat(xi, yi + 1, per, seed) + (DpLat(xi + 1, yi + 1, per, seed) - DpLat(xi, yi + 1, per, seed)) * sx;
    return a + (b - a) * sy;
}
static void DpFogTexBuild(void)
{
    static float n[DP_TEX_N * DP_TEX_N];
    static int hist[1024];
    float lo = 0.0f, hi = 1.0f;
    int x, y, i, acc;
    for (y = 0; y < DP_TEX_N; ++y)
        for (x = 0; x < DP_TEX_N; ++x) {
            float u = ((float)x + 0.5f) / DP_TEX_N, v = ((float)y + 0.5f) / DP_TEX_N, s = 0.0f, amp = 1.0f, tot = 0.0f;
            int o;
            u += 0.10f * (DpPNoise(u, v, 4, 0xD9A1u) - 0.5f) * 2.0f;
            v += 0.10f * (DpPNoise(u, v, 4, 0xD9A2u) - 0.5f) * 2.0f;
            for (o = 0; o < 4; ++o) {
                s += amp * DpPNoise(u, v, 4 << o, 0xD9B0u + (DWORD)o * 977u);
                tot += amp;
                amp *= 0.5f;
            }
            n[y * DP_TEX_N + x] = s / tot;
        }
    memset(hist, 0, sizeof hist);  // the 2nd / 98th percentiles (as the reference) from a histogram
    for (i = 0; i < DP_TEX_N * DP_TEX_N; ++i) ++hist[(int)(FxSat(n[i]) * 1023.0f)];
    for (i = 0, acc = 0; i < 1024; ++i)
        if ((acc += hist[i]) >= DP_TEX_N * DP_TEX_N / 50) { lo = (float)i / 1023.0f; break; }
    for (i = 1023, acc = 0; i >= 0; --i)
        if ((acc += hist[i]) >= DP_TEX_N * DP_TEX_N / 50) { hi = (float)i / 1023.0f; break; }
    if (!(hi > lo + 0.05f)) lo = 0.0f, hi = 1.0f;
    for (i = 0; i < DP_TEX_N * DP_TEX_N; ++i) {
        const float t = FxSat((n[i] - lo) / (hi - lo));
        g_dpTexA[i] = (BYTE)(FxSmooth(DP_TEX_LO, DP_TEX_HI, t) * 255.0f + 0.5f);
        g_dpTexC[i] = (BYTE)(FxSmooth(DP_HI_LO, DP_HI_HI, t) * 255.0f + 0.5f);
        g_dpTexN[i] = (BYTE)(t * 255.0f + 0.5f);
    }
    g_dpTexBuilt = 1;
}
static float DpFogN(float u, float v)  // the raw noise 0..1, bilinear, wrapping (CPU side: tints, wisps)
{
    const float x = u * DP_TEX_N - 0.5f, y = v * DP_TEX_N - 0.5f, fx0 = floorf(x), fy0 = floorf(y), fx = x - fx0, fy = y - fy0;
    const int x0 = (int)fx0 & (DP_TEX_N - 1), y0 = (int)fy0 & (DP_TEX_N - 1), x1 = (x0 + 1) & (DP_TEX_N - 1), y1 = (y0 + 1) & (DP_TEX_N - 1);
    float a, b;
    if (!g_dpTexBuilt) DpFogTexBuild();
    a = g_dpTexN[y0 * DP_TEX_N + x0] + (g_dpTexN[y0 * DP_TEX_N + x1] - g_dpTexN[y0 * DP_TEX_N + x0]) * fx;
    b = g_dpTexN[y1 * DP_TEX_N + x0] + (g_dpTexN[y1 * DP_TEX_N + x1] - g_dpTexN[y1 * DP_TEX_N + x0]) * fx;
    return (a + (b - a) * fy) / 255.0f;
}
// FXX_WISP (PLAN 8.3, renders/NOTES.md v3): cell c = x / 64 (curl phase 1.57 c), y up from the bottom row: the S-curled
// tapered strand (taper exponent 1.2: a softer tip), the fog noise inside, premultiplied grey
static DWORD DpWispTexel(int x, int y)
{
    const int c = x >> 6;
    const float qx = ((float)(x & 63) + 0.5f) / 32.0f - 1.0f, qy = 1.0f - ((float)y + 0.5f) / 64.0f;  // -1..1, y up
    const float ph = 1.5708f * (float)c, cen = 0.275f * sinf(2.4f * qy + ph);
    const float wy = 1.0f - qy * qy, w = 0.5f * (wy > 0.0f ? powf(wy, 1.2f) : 0.0f);
    const float n = DpFogN(0.5f + 0.35f * qx + 0.1f * ph, 0.5f + 0.08f * qy + 0.07f * ph);
    float a = 0.0f;
    if (w > 0.001f) {
        a = 1.0f - FxSmooth(0.15f, 1.0f, fabsf(qx - cen) / w * (0.85f + 0.5f * (n - 0.5f)));
        a *= FxSmooth(-1.0f, -0.55f, qy) * (1.0f - FxSmooth(0.3f, 1.0f, qy)) * (0.55f + 0.9f * n) / 1.45f;
    }
    {
        const DWORD b = (DWORD)(FxSat(a) * 255.0f + 0.5f);
        return b << 24 | b << 16 | b << 8 | b;
    }
}
// v3.27.1 FXX_SMOKE (PLAN 2.4; renders/NOTES_v5.md 1-3): 1024 x 512, 4 x 2 cells of 256, premultiplied grey.
// The billow noise n = 0.55 N(0.34 q) + 0.45 N(1.00 q) of the fog texture (q -1..1 across the cell, an offset per cell);
// the lobed outline rr' = rr (0.62 + 1.00 (n - 0.5)), the profile (1 - smoothstep(core, 1, rr')) (1 - smoothstep(0.8, 1, rr)),
// holes: alpha x (0.04 + 1.90 smoothstep(0.36, 0.56, n)).
//   0, 1 DPS_SMOKE: core 0.45; RGB = A x (0.55 + 1.00 n) / 1.55 (the billow shading; the colour is given x 1.55)
//   2, 3 DPS_CREST: the crest light of cells 0 / 1 (additive): profile x smoothstep(0.60, 0.95, n)
//   4 DPS_HALO: core 0.55 (the black glow's flat middle); 5 DPS_SOFT: a plain soft disc, core 0.2 (no holes)
//   6, 7 DPS_STRAND: across u the strip profile 1 - smoothstep(0.15, 1, |u|) (ribbons)
static DWORD DpSmokeTexel(int x, int y)
{
    const int c = (x >> 8) + 4 * (y >> 8);
    const float qx = ((float)(x & 255) + 0.5f) / 128.0f - 1.0f, qy = ((float)(y & 255) + 0.5f) / 128.0f - 1.0f;
    const float rr = sqrtf(qx * qx + qy * qy), edge = 1.0f - FxSmooth(0.8f, 1.0f, rr);
    float a, g;
    if (c >= 6) {
        a = g = 1.0f - FxSmooth(0.15f, 1.0f, fabsf(qx));
    } else if (c == 5) {
        a = g = (1.0f - FxSmooth(0.2f, 1.0f, rr)) * edge;
    } else {
        const int src = c == 4 ? 0 : c & 1;  // the crest cells share their smoke cell's noise
        const float o = 0.31f * (float)src + (c == 4 ? 0.53f : 0.0f);
        const float n = 0.55f * DpFogN(0.34f * qx + 0.50f + o, 0.34f * qy + 0.50f - o) + 0.45f * DpFogN(1.00f * qx + 0.87f - o, 0.70f * qy + 0.71f + o);
        const float core = c == 4 ? 0.55f : 0.45f;
        const float prof = (1.0f - FxSmooth(core, 1.0f, rr * (0.62f + 1.00f * (n - 0.5f)))) * edge;
        a = c == 4 ? FxSat(prof * (0.85f + 0.15f * n))  // v3.27.2 (user item 4): the cloak is smooth (no holes)
                   : FxSat(prof * (0.04f + 1.90f * FxSmooth(0.36f, 0.56f, n)));
        if (c == 2 || c == 3) a = g = FxSat(prof * FxSmooth(0.60f, 0.95f, n));
        else g = c == 4 ? a : a * (0.55f + 1.00f * n) / 1.55f;
    }
    {
        const DWORD A = (DWORD)(FxSat(a) * 255.0f + 0.5f), G = (DWORD)(FxSat(g) * 255.0f + 0.5f);
        return A << 24 | G << 16 | G << 8 | G;
    }
}
static DWORD DpFogTexel(int x, int y)
{
    const int i = (y & (DP_TEX_N - 1)) * DP_TEX_N + (x & (DP_TEX_N - 1));
    if (!g_dpTexBuilt) DpFogTexBuild();
    return (DWORD)g_dpTexA[i] << 24 | (DWORD)g_dpTexC[i] << 16 | (DWORD)g_dpTexC[i] << 8 | g_dpTexC[i];
}

// ---------------------------------------------------------------- the beings' models (0x52AEB5, every registry draw)
typedef struct { const BYTE *task; BYTE *model; } DpMdl;
static DpMdl g_dpMdl[DP_ART_MAX];
static int g_dpMdlN;
static LONG g_dpHerFrame = -1;  // v3.27.1: her (the player's) model was drawn in this frame (PLAN 3.1)
static LONG g_dpMdlStamp = -1;
static Viewport8 g_dpVp;
static int g_dpVpOk;
static void DpCollect(const BYTE *task, BYTE *model)
{
    int i;
    if (!g_dpInstalled || g_dpFail || g_dpLevel <= 0 || !task || !model) return;
    if (g_dpMdlStamp != g_fxFrame) {  // the first body of a frame: a new list, and the 3D viewport in force
        void *dev = DEV;
        g_dpMdlStamp = g_fxFrame;
        g_dpMdlN = 0;
        g_dpVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_dpVp)) && g_dpVp.w >= 16 && g_dpVp.h >= 16;
    }
    if (model == *(BYTE *const volatile *)(0x10A0F20 + 0x1B0)) g_dpHerFrame = g_fxFrame;  // v3.27.1: her aura's bones
    for (i = 0; i < g_dpArtN; ++i)
        if (g_dpArt[i].task == task) {
            if (g_dpMdlN < DP_ART_MAX) g_dpMdl[g_dpMdlN].task = task, g_dpMdl[g_dpMdlN++].model = model;
            return;
        }
}
static BYTE *DpModelOf(const BYTE *task, int kind)
{
    int i;
    (void)kind;
    // review DP-R602 (supersedes the DP-R402 fallback): only a model drawn this frame (collected at 0x52AEB5, which the
    // install now requires): a being not drawn this frame gets no body cue (KB 127: no bones this frame, nothing body-bound)
    if (g_dpMdlStamp == g_fxFrame)
        for (i = 0; i < g_dpMdlN; ++i)
            if (g_dpMdl[i].task == task) return g_dpMdl[i].model;
    return NULL;
}

// ---------------------------------------------------------------- frame state
static float g_dpNow, g_dpDt, g_dpCen[3], g_dpWall[DP_WALLS], g_dpWallAge[DP_WALLS], g_dpWallRaw[DP_WALLS];  // Raw: unclamped (puffs)  // (g_dpDt: this frame's clock step)
static int g_dpCenOk, g_dpWallNext;
static LONG g_dpDrawFrame = -100;
static LARGE_INTEGER g_dpQpc;
static DWORD g_dpDrawRoom = 0xFFFFFFFFu;
static float DpClock(void)  // real seconds, advanced only across consecutive drawn frames; returns this frame's dt
{
    LARGE_INTEGER now, f;
    float dt = 0.0f;
    QueryPerformanceCounter(&now);
    if (g_dpDrawFrame == g_fxFrame - 1 && QueryPerformanceFrequency(&f) && f.QuadPart)
        dt = (float)((double)(now.QuadPart - g_dpQpc.QuadPart) / (double)f.QuadPart);
    if (!(dt >= 0.0f)) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;
    g_dpQpc = now;
    g_dpDrawFrame = g_fxFrame;
    g_dpNow += dt;
    return dt;
}
// the wall distances round her (8 casts a frame, round robin; a ray at knee height out to 1.2 R)
static void DpWalls(const float c[3], float R, float dt)
{
    int k;
    for (k = 0; k < DP_WALLS; ++k) g_dpWallAge[k] += dt;
    for (k = 0; k < 8; ++k) {
        const int i = g_dpWallNext;
        const float a = 6.2831853f * (float)i / DP_WALLS;
        float p0[3], p1[3], hit[3], nrm[3];
        g_dpWallNext = (g_dpWallNext + 1) % DP_WALLS;
        if (g_dpWallAge[i] < 0.25f && g_dpWall[i] > 0.0f) continue;
        {   // v3.27.1 (user item 5): two rays, knee and chest height; the FARTHER hit counts (a railing or a prop blocks
            // only the low ray, a wall both); never closer than 0.35 R, so the fog cannot vanish round her entirely
            float best = 0.0f;
            int hgt;
            for (hgt = 0; hgt < 2; ++hgt) {
                float d = 9.0e9f;
                p0[0] = c[0], p0[1] = c[1] - (hgt ? 140.0f : 40.0f), p0[2] = c[2];
                p1[0] = c[0] + cosf(a) * 1.2f * R, p1[1] = p0[1], p1[2] = c[2] + sinf(a) * 1.2f * R;
                if (SvvCast(p0, p1, hit, nrm)) d = sqrtf((hit[0] - p0[0]) * (hit[0] - p0[0]) + (hit[2] - p0[2]) * (hit[2] - p0[2]));
                if (d > best) best = d;
            }
            g_dpWall[i] = best > 0.35f * R ? best : 0.35f * R;
            g_dpWallRaw[i] = best;  // review DP3271-R5-001: the puffs use the real distance (the slab keeps its 0.35 R floor)
        }
        g_dpWallAge[i] = 0.0f;
    }
}
static float DpRimNoise(float th, float t, float k)
{
    return 0.55f * sinf(3.0f * th + 0.35f * t + k) + 0.30f * sinf(5.0f * th - 0.5f * t + 2.0f * k + 1.0f) +
           0.15f * sinf(8.0f * th + 0.8f * t + 3.0f * k);
}

// ---------------------------------------------------------------- the slab mesh
static SpVtx g_dpV[2 * 2 * 3 * DP_RINGS * DP_SECTS];  // (a clipped triangle can become two)
typedef struct { float h[4], u, v, a, c[3]; } DpCV;  // a disc vertex before projection
static void DpCVLerp(DpCV *o, const DpCV *a, const DpCV *b, float t)
{
    int k;
    for (k = 0; k < 4; ++k) o->h[k] = a->h[k] + (b->h[k] - a->h[k]) * t;
    for (k = 0; k < 3; ++k) o->c[k] = a->c[k] + (b->c[k] - a->c[k]) * t;
    o->u = a->u + (b->u - a->u) * t, o->v = a->v + (b->v - a->v) * t, o->a = a->a + (b->a - a->a) * t;
}
static int DpClipNear(const DpCV in[3], DpCV out[4])  // Sutherland-Hodgman against w >= GV_NEAR_W: 0, 3 or 4 vertices
{
    int k, n = 0;
    for (k = 0; k < 3; ++k) {
        const DpCV *a = &in[k], *b = &in[(k + 1) % 3];
        const int ia = a->h[3] >= GV_NEAR_W, ib = b->h[3] >= GV_NEAR_W;
        if (ia) out[n++] = *a;
        if (ia != ib && n < 4) DpCVLerp(&out[n++], a, b, (GV_NEAR_W - a->h[3]) / (b->h[3] - a->h[3]));
    }
    return n >= 3 ? n : 0;
}
static int DpEmit(SpVtx *v, const DpCV *c, const Viewport8 *vp, int mode)
{
    float x, y, h[4];
    memcpy(h, c->h, sizeof h);
    if (h[3] < GV_NEAR_W) h[3] = GV_NEAR_W;  // (a clipped vertex sits on the plane: rounding)
    if (!GvScreen(h, vp, &x, &y)) return 0;
    v->x = x - 0.5f;
    v->y = y - 0.5f;
    v->z = SvZH(h);
    v->rhw = 1.0f / h[3];
    v->u = c->u;
    v->v = c->v;
    if (mode == 0) {  // night vision: colour x 0 (v3.27.2: pitch black), alpha x 1.3 (at most 0.9); Thermal: x 0.08 (black)
        const float cm = g_dpDiscLook == 2 ? 0.0f : g_dpDiscLook == 1 ? 0.08f : 1.0f, a = g_dpDiscLook == 2 ? (c->a * 1.3f < 0.9f ? c->a * 1.3f : 0.9f) : c->a;
        v->c = (DWORD)(FxSat(a) * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(c->c[0] * cm) * 255.0f + 0.5f) << 16 |
               (DWORD)(FxSat(c->c[1] * cm) * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(c->c[2] * cm) * 255.0f + 0.5f);
    } else {
        const float g = c->a * g_dpHiGain;
        v->c = 0xFF000000u | (DWORD)(FxSat(g_dpHiRgb[0] * g) * 255.0f + 0.5f) << 16 |
               (DWORD)(FxSat(g_dpHiRgb[1] * g) * 255.0f + 0.5f) << 8 | (DWORD)(FxSat(g_dpHiRgb[2] * g) * 255.0f + 0.5f);
    }
    return 1;
}
// one polar disc at world height y round (cx, cz) out to rmax: per-vertex mask x layer density; mode 0 body (alpha
// blend, the texture's alpha), 1 highlights (additive, the texture's colour). Returns the triangles drawn.
static int DpDisc(void *dev, const Viewport8 *vp, float cx, float y, float cz, float rmax, int rings, int sects,
                  const DpLayerK *L, float dens, int mode, int walls, float rimK, float boostOf)
{
    static float hv[DP_RINGS + 1][DP_SECTS + 1][4];
    static float al[DP_RINGS + 1][DP_SECTS + 1];
    static float tu[DP_RINGS + 1][DP_SECTS + 1], tv[DP_RINGS + 1][DP_SECTS + 1];
    static float tc[DP_RINGS + 1][DP_SECTS + 1][3];  // v3: the slow violet / blue-violet / magenta tint (PLAN 8.3)
    const float t = g_dpNow, ang = L->rot * t, ca = cosf(ang), sa = sinf(ang);
    const float e1 = 1.0f, e0 = DP_MASK_IN / DP_MASK_OUT;
    int i, j, n = 0;
    if (rings > DP_RINGS) rings = DP_RINGS;
    if (sects > DP_SECTS) sects = DP_SECTS;
    for (i = 0; i <= rings; ++i)
        for (j = 0; j <= sects; ++j) {
            const float th = 6.2831853f * (float)(j % sects) / (float)sects, r = rmax * (float)i / (float)rings;
            float p[3], m, shrink, d, wx, wz;
            p[0] = cx + r * cosf(th), p[1] = y, p[2] = cz + r * sinf(th);
            shrink = rimK >= 0.0f ? 1.0f - DP_RIM_AMP * (0.5f + 0.5f * DpRimNoise(th, t, rimK)) : 1.0f;
            m = 1.0f - FxSmooth(e0 * shrink, e1 * shrink, r / rmax);
            if (walls) {
                const float hit = g_dpWall[(j % sects) * DP_WALLS / sects];
                m *= 1.0f - FxSmooth(0.0f, 1.0f, (r - (hit - DP_WALL_PRE)) / DP_WALL_LEN);
            }
            if (g_svEyeOk) {
                const float dx = p[0] - g_svEye[0], dy = p[1] - g_svEye[1], dz = p[2] - g_svEye[2];
                d = sqrtf(dx * dx + dy * dy + dz * dz);
                m *= FxSmooth(DP_NEAR0, DP_NEAR1, d);  // v3.27.1 (PLAN 2.2)
            }
            if (walls && g_dpHoleOn) {  // v3.27.1: her shins are not veiled (density x (1 - 0.85 (1 - smoothstep(25, 85, r))))
                const float hx = p[0] - g_dpHoleC[0], hz = p[2] - g_dpHoleC[2];
                m *= 1.0f - 0.85f * (1.0f - FxSmooth(25.0f, 85.0f, sqrtf(hx * hx + hz * hz)));
            }
            if (boostOf > 0.0f) {  // the main layers gather round each victim (PLAN 8.3)
                int q;
                float b = 0.0f;
                for (q = 0; q < g_dpArtN; ++q) {
                    const float vx = p[0] - g_dpArt[q].pos[0], vz = p[2] - g_dpArt[q].pos[2];
                    b += g_dpArt[q].r * expf(-(vx * vx + vz * vz) / (170.0f * 170.0f));
                }
                m *= 1.0f + boostOf * b;
                if (m > 1.0f) m = 1.0f;
            }
            al[i][j] = m * dens;
            if (mode == 0) {
                static const float kBlue[3] = { 0.010f, 0.014f, 0.062f }, kMag[3] = { 0.052f, 0.008f, 0.046f };
                const float tn = FxSmooth(0.25f, 0.75f, DpFogN(p[0] / 900.0f + 0.37f + 0.004f * t, p[2] / 900.0f + 0.11f - 0.0028f * t));
                const float bright = 1.0f + 0.4f * FxSat(m);
                int c3;
                for (c3 = 0; c3 < 3; ++c3)
                    tc[i][j][c3] = (tn < 0.5f ? kDpBody[c3] + (kBlue[c3] - kDpBody[c3]) * (1.0f - 2.0f * tn)
                                              : kDpBody[c3] + (kMag[c3] - kDpBody[c3]) * (2.0f * tn - 1.0f)) * bright;
            }
            wx = p[0], wz = p[2];
            tu[i][j] = ((wx * ca - wz * sa) + L->sx * t) / L->scale;
            tv[i][j] = ((wx * sa + wz * ca) + L->sz * t) / L->scale;
            GvXform(p, hv[i][j]);
        }
    for (i = 0; i < rings; ++i)
        for (j = 0; j < sects; ++j) {
            static const int kT[2][3][2] = { { { 0, 0 }, { 1, 0 }, { 0, 1 } }, { { 1, 0 }, { 1, 1 }, { 0, 1 } } };
            int tri;
            if (al[i][j] < 0.004f && al[i + 1][j] < 0.004f && al[i][j + 1] < 0.004f && al[i + 1][j + 1] < 0.004f) continue;
            for (tri = 0; tri < 2; ++tri) {  // review DP-R403: each triangle clipped against the near plane (w = GV_NEAR_W)
                DpCV in[3], out[4];
                int q, no;
                for (q = 0; q < 3; ++q) {
                    const int a = i + kT[tri][q][0], b = j + kT[tri][q][1];
                    memcpy(in[q].h, hv[a][b], sizeof in[q].h);
                    in[q].u = tu[a][b], in[q].v = tv[a][b], in[q].a = FxSat(al[a][b]);
                    memcpy(in[q].c, tc[a][b], sizeof in[q].c);
                }
                no = DpClipNear(in, out);
                for (q = 1; q + 1 < no && n + 3 <= (int)(sizeof g_dpV / sizeof g_dpV[0]); ++q)
                    if (DpEmit(&g_dpV[n], &out[0], vp, mode) && DpEmit(&g_dpV[n + 1], &out[q], vp, mode) &&
                        DpEmit(&g_dpV[n + 2], &out[q + 1], vp, mode))
                        n += 3;
            }
        }
    if (n) ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), g_dpV, sizeof(SpVtx));
    return n / 3;
}
static void DpStates(void *dev, void *tex, int mode)  // 0 body: colour = diffuse, alpha = tex.a x diffuse.a; 1 additive
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    FxSampler(dev, tex, 1, 0);                    // wrap, linear
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, mode ? 4 /*MODULATE*/ : 3 /*SELECTARG2*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 1 /*COLOROP*/, 1 /*DISABLE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, mode ? 2 /*ONE*/ : 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, mode ? 2 /*ONE*/ : 6 /*INVSRCALPHA*/);
}

// ---------------------------------------------------------------- sprites: wisps and victims (PLAN 8.3)
#include "deathpresence_sprites.inc"

// ---------------------------------------------------------------- v3.35 FX335: made ready (echo.inc RfWarmFrame)
// Once a device with the row on: its five textures (fog, wisps, smoke, the atlas, the flame flipbook) and its draw states
// (the sprites' four textures x premultiplied over / additive, the pool's body and additive modes), each drawn once as a
// zero-colour quad (FxWarmQuad: nothing shows). After Shock's and Echo's sprites and ribbons use the same states.
static void *g_dpWarmDev;
static int DpWarmStates(void *dev)  // the caller set FxBase and SvZ; returns the quads drawn
{
    static const int kTex[4] = { FXX_SVATLAS, FXX_SMOKE, FXX_FLAME, FXX_WISP };
    void *fog;
    int i, ad, n = 0;
    for (i = 0; i < 4; ++i) {
        void *tex = FxTexture(dev, kTex[i]);
        if (!tex) continue;
        for (ad = 0; ad < 2; ++ad) DpSlStates(dev, tex, ad), FxWarmQuad(dev), ++n;
    }
    if ((fog = FxTexture(dev, FXX_FOG)) != NULL)
        for (ad = 0; ad < 2; ++ad) DpStates(dev, fog, ad), FxWarmQuad(dev), ++n;
    return n;
}
static int DpWarm(void *dev, int *quads)  // 1 done now
{
    FxSaved sv;
    *quads = 0;
    if (g_dpWarmDev == dev || !g_dpInstalled || g_dpFail || g_dpLevel <= 0) return 0;
    if (!FxSave(dev, &sv, 0)) return 0;
    __try {
        FxBase(dev);
        SvZ(dev, 2);
        *quads = DpWarmStates(dev);
    } __finally {
        FxRestore(dev, &sv);
    }
    g_dpWarmDev = dev;
    return 1;
}

// ---------------------------------------------------------------- the frame
static int g_dpDrawFaults, g_dpDrawMiss, g_dpDrawMissTotal;
static LONG g_dpSvFrame = -1;     // v3.27.1: Spirit Vision is on this frame: DpSvDraw (from GvPass) draws the fog's look
static LONG g_dpSvDone = -1;      // ... the last frame DpSvDraw drew
static DWORD g_dpVisit = 1;              // +1 at every room change (DpFrameBegin): one visit of one room
static DWORD g_dpSvAskVisit, g_dpSvBroken[4];  // (v3.27.2, review DP3272-002: broken per look)  // the visit of the pending request; the visit where GvPass left one unserved:
                                            // the full look for the rest of that visit (review DP3271-R2-003 / R3-001)
// review DP3271-R2-001: a failed draw is named at once (once per room and cause), before any 300-frame shutdown
static void DpMissLog(DWORD room, const char *what)
{
    static DWORD s_room = 0xFFFFFFFFu;
    static const char *s_what;
    if (room == s_room && what == s_what) return;
    s_room = room, s_what = what;
    Log("death presence: stage %lX room %lX: no fog this frame: %s unavailable (misses %d in a row, %d in all)", room >> 16,
        room & 0xFFFF, what, g_dpDrawMiss + 1, g_dpDrawMissTotal);
}
static float g_dpMsMax;           // the slowest fog frame (CPU ms) since the room's log line (B10)
static int g_dpPostLook = 3;      // v3.27.2: the look the pending post-filter request asks for (3 SV, 1 Thermal, 2 NV)
static int g_dpProjSrc;           // v3.27.2 (PLAN 2): the projection the fog's depth used: 1 the camera's, 2 SvNearFar's, 0 none
static float g_dpProjUsed[2], g_dpProjOther[2];  // its m22 / m32, and SvNearFar's choice before the override
// v3.27.2 (user items 1 / 3, PLAN 2.2): the fog's depth from the camera's own projection (0x12E3ED0, cam+0xA0), not from
// 0xF4E760 (flipY x proj of the last model drawn: at the HUD hook it can belong to any model). After SvNearFar.
static void DpCamProj(void)
{
    float P[16];
    g_dpProjOther[0] = g_svPOk ? g_svP[10] : 0.0f, g_dpProjOther[1] = g_svPOk ? g_svP[14] : 0.0f;
    __try {
        memcpy(P, (const void *)0x12E3ED0, sizeof P);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_dpProjSrc = g_svPOk ? 2 : 0;
        return;
    }
    if (_finite(P[10] + P[11] + P[14] + P[15]) && fabsf(P[11]) > 0.5f && P[10] != 0.0f && P[10] != 1.0f) {
        memcpy(g_svP, P, sizeof g_svP);
        g_svPOk = 1;
        g_dpProjSrc = 1;
    } else {
        g_dpProjSrc = g_svPOk ? 2 : 0;
    }
    g_dpProjUsed[0] = g_svPOk ? g_svP[10] : 0.0f, g_dpProjUsed[1] = g_svPOk ? g_svP[14] : 0.0f;
    g_svZScaleNow = g_dpZExact ? 1.0f : SV_ZSCALE;  // v3.27.3 (re_depth.md): the exact stored depth; back after the fog's draw
}
static float g_dpMsPart;          // review DP3271-I03: the scene pass's part of a Spirit Vision frame (clock, walls) ...
static LONG g_dpMsPartFrame = -1; // ... and its frame: DpSvDraw adds it to its own
static int DpLookNow(void)        // PLAN 5: 3 Spirit Vision, 1 Thermal, 2 night vision (both), 0 the full look
{
    const int f = g_fxOn ? (int)g_fx[FX_FILTER] : 0;
    return f == FXF_GHOST ? 3 : f == FXF_THERMAL ? 1 : (f == FXF_NV || f == FXF_NVMONO) ? 2 : 0;
}
// v3.41 DP341 (USER_FEEDBACK_v340 "Added by the user" 1: spikes through the peepholes and in the walkways at a new room's
// load): the draw gate (dp_sim.h DpGateStep). The Room 302 door scope / wall hole views (0x4D74E0 / 0x4D8000 / the
// hole events of 0x4D8390, no cut-scene: game phase 0), the window look-out (0x54C190), the Water Prison peep (0x4E8CE0)
// turn the room's geometry off (RoomGeometryDrawOff 0x4F0870: [0xFD6C78] = 0) and show another place through their own
// camera, so the fog round her stood in that view with nothing in front of it; a room exit clears the same flag
// (0x4F0CF0) and the room's load sets it (0x4F0AC0), whose entry place reaches her position only at the player's next
// start ([0x10A12FC] warp, 0x52CEE0), a frame or more after the room number changed: the wisps (anchored for 4-7 s where
// the fog's centre was at their birth) then stood at her old place in the new room. Read-only signals, no exe site.
static DpGate g_dpGate;
static volatile LONG g_dpGateQ, g_dpGateAnomQ;
static int DpGateFrame(const float pp[3], DWORD room, float dt)  // 1 = draw this frame
{
    static int s_longLogged;
    DpGateIn in;
    int reset, show;
    in.room = room;
    in.roomDrawn = *(volatile int *)0xFD6C78 != 0;  // gRoomModels.drawn (decomp include/room_load.h)
    in.peep = *(volatile int *)0xFD5BE8 != 0;       // gPeepFlag188 (include/event_peep.h)
    in.warp = *(volatile int *)0x10A12FC != 0;      // gPlayer[0].warp (include/player.h +0x3DC)
    memcpy(in.pos, pp, sizeof in.pos);
    show = DpGateStep(&g_dpGate, &in, dt, &reset);
    if (g_dpGate.changed) {
        s_longLogged = 0;
        if (LogQuota(&g_dpGateQ, 600))
            LogV("death presence: stage %lX room %lX: hidden: %s", room >> 16, room & 0xFFFF, DpGateWhy(g_dpGate.why));
    }
    if (g_dpGate.warpCapped && LogQuota(&g_dpGateQ, 600))
        LogV("death presence: stage %lX room %lX: a move of the player stayed pending for %.0f s: no longer waited for",
             room >> 16, room & 0xFFFF, DP_GATE_WARP_MAX);
    if (!show && !s_longLogged && g_dpGate.heldS > 20.0f) {  // once per hold: a "the fog vanished" report names its cause
        s_longLogged = 1;
        if (g_dpGate.why == DPW_ROOM || g_dpGate.why == DPW_JUMP || g_dpGate.why == DPW_BADPOS) {  // never more than a frame
            if (LogQuota(&g_dpGateAnomQ, 5))
                LogAnomaly("death presence", "stage %lX room %lX: hidden for 20 s by %s (her position %.0f %.0f %.0f keeps "
                           "changing room or jumping)", room >> 16, room & 0xFFFF, DpGateWhy(g_dpGate.why), pp[0], pp[1], pp[2]);
        } else if (LogQuota(&g_dpGateQ, 600)) {
            LogV("death presence: stage %lX room %lX: still hidden after 20 s: %s (her model drawn %s)", room >> 16,
                 room & 0xFFFF, DpGateWhy(g_dpGate.why), g_dpHerFrame == g_fxFrame ? "yes" : "no");
        }
    }
    if (reset) {  // every anchored piece starts again at her: DpFrameBegin's new-visit branch (centre, walls, DpSprReset)
        g_dpCenOk = 0;
        if (LogQuota(&g_dpGateQ, 600))
            LogV("death presence: stage %lX room %lX: shown again after %.2f s hidden (%s; still for %d frames)", room >> 16,
                 room & 0xFFFF, g_dpGate.heldS, DpGateWhy(g_dpGate.why), g_dpGate.frames);
    }
    return show;
}
// v3.42 SM342 (USER_FEEDBACK_v341: Echo Doubles and After Shock hidden in peep / window views, the fog's signals): one
// drawn frame of an effect through its view gate (dp_sim.h DpViewStep), called by echo.inc EchoDrawC and afterimage.inc
// AiDrawC before they sample or draw anything. Real time between consecutive calls (a gap: 0). LogV when it hides and
// when it shows again (a shared quota: about 2 lines a view). 1 = draw this frame.
typedef struct { DpView g; LARGE_INTEGER qpc; LONG frame; } DpViewFx;
static volatile LONG g_dpViewQ;
static int DpViewFrame(DpViewFx *v, const char *who)
{
    LARGE_INTEGER now, f;
    float dt = 0.0f;
    int show;
    if (v->frame == g_fxFrame) return v->g.state == DPV_SHOW;  // (twice in one frame: no second step)
    QueryPerformanceCounter(&now);
    if (v->frame == g_fxFrame - 1 && QueryPerformanceFrequency(&f) && f.QuadPart)
        dt = (float)((double)(now.QuadPart - v->qpc.QuadPart) / (double)f.QuadPart);
    v->qpc = now;
    v->frame = g_fxFrame;
    show = DpViewStep(&v->g, *(volatile int *)0xFD6C78 != 0, *(volatile int *)0xFD5BE8 != 0, *(volatile int *)0x10A12FC != 0, dt);
    if (v->g.changed && LogQuota(&g_dpViewQ, 600))
        LogV("%s: hidden in a view: %s (stage %lX room %lX)", who, DpGateWhy(v->g.why), *(volatile DWORD *)0xFD5A90,
             *(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (v->g.warpCapped && LogQuota(&g_dpViewQ, 600))
        LogV("%s: a move of the player stayed pending for %.0f s: no longer waited for", who, DP_GATE_WARP_MAX);
    if (v->g.resumed && LogQuota(&g_dpViewQ, 600))
        LogV("%s: shown again after %.2f s hidden (%s)", who, v->g.heldS, DpGateWhy(v->g.why));
    return show;
}
// the frame's common part: the clock, the lagging centre (a new room: at her, the walls measured again, every slot
// re-anchored), the wall rays. 0 = nothing to draw (no position, or the gate holds: v3.41 DP341).
static int DpFrameBegin(float pp[3], DWORD *roomOut, float R)
{
    const float dt = DpClock();
    DWORD room;
    int k;
    g_dpDt = dt;
    memcpy(pp, (const void *)0x10A0F60, 12);
    room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    if (!DpGateFrame(pp, room, dt)) return 0;  // v3.41 DP341: a view of another place, a load, a room change settling
    if (!DpFinite3(pp)) return 0;
    {   // v3.28 (user v3.27.3 item 4: the floor pool slid in from the next area's centre on entering a room): her position
        // can still be stale when the room number changes (it jumps a frame or more later), so for 1 s after a new room,
        // and on any jump of more than 150 u in a frame, the centre snaps to her (walls measured again) instead of lagging
        static float s_snapUntil = -1.0f, s_prev[3];
        static int s_prevOk;
        const float jx = pp[0] - s_prev[0], jy = pp[1] - s_prev[1], jz = pp[2] - s_prev[2];
        const int jump = s_prevOk && jx * jx + jy * jy + jz * jz > 150.0f * 150.0f;
        memcpy(s_prev, pp, sizeof s_prev);
        s_prevOk = 1;
        if (room != g_dpDrawRoom || !g_dpCenOk) s_snapUntil = g_dpNow + 1.0f;
        if (room == g_dpDrawRoom && g_dpCenOk && (jump || g_dpNow < s_snapUntil)) {
            memcpy(g_dpCen, pp, sizeof g_dpCen);
            if (jump) for (k = 0; k < DP_WALLS; ++k) g_dpWall[k] = g_dpWallRaw[k] = 9.0e9f, g_dpWallAge[k] = 1.0f;
        }
    }
    if (room != g_dpDrawRoom || !g_dpCenOk) {
        g_dpDrawRoom = room;
        ++g_dpVisit;  // a new visit: Spirit Vision is asked again (review DP3271-R3-001)
        memcpy(g_dpCen, pp, sizeof g_dpCen);
        g_dpCenOk = 1;
        for (k = 0; k < DP_WALLS; ++k) g_dpWall[k] = g_dpWallRaw[k] = 9.0e9f, g_dpWallAge[k] = 1.0f;
        DpSprReset();
        g_dpMsMax = 0.0f;  // review DP3271-R5-002: the room's timing window opens before its first measured frame
    } else {  // (during the snap window g_dpCen == pp already: the lag below changes nothing)
        const float a = 1.0f - expf(-dt / DP_LAG_S);
        for (k = 0; k < 3; ++k) g_dpCen[k] += (pp[k] - g_dpCen[k]) * a;
    }
    DpWalls(g_dpCen, R, dt);
    *roomOut = room;
    return 1;
}
// the floor pool: the 4 u and 30 u discs (body), the victims' slabs, the 30 u crest highlights (full look only)
static int DpPool(void *dev, const Viewport8 *vp, void *tex, float R, int level, const float pp[3])
{
    const DpLook *L = &kDpLook[level];
    const float rmax = DP_MASK_OUT * R;
    int k, q, tris = 0;
    g_dpHoleOn = 0;  // v3.27.2 (user item 2): the pool reaches her feet (the v5 shin hole is gone)
    memcpy(g_dpHoleC, pp, sizeof g_dpHoleC);
    g_dpDiscLook = g_dpLook;
    DpStates(dev, tex, 0);
    for (k = 0; k < DP_NLAYER; ++k) {
        tris += DpDisc(dev, vp, g_dpCen[0], g_dpCen[1] - kDpLayer[k].h, g_dpCen[2], rmax, DP_RINGS, DP_SECTS, &kDpLayer[k],
                       k ? L->pool30 : L->pool4, 0, 1, 2.1f * (float)k, 0.9f);
        if (k == 0)
            for (q = 0; q < g_dpArtN; ++q) {  // review DP-R502: every victim (at most DP_ART_MAX); v3.27.1: r = its stain
                const DpArt *a = &g_dpArt[q];
                const float r = a->r, rad = (90.0f + 50.0f * r) * (0.5f + 0.5f * r);
                if (!(r > DP_STAIN_MIN)) continue;
                tris += DpDisc(dev, vp, a->pos[0], a->pos[1] - 12.0f, a->pos[2], rad, 8, 24, &kDpLayer[1],
                               0.70f * (0.5f + 0.5f * r) * DpStainFade(r) * FxSmooth(DP_STAIN_MIN, 0.40f, r), 0, 0, -1.0f, 0.0f);  // DP3271-001: to 0 with the stain
            }
    }
    if (g_dpLook == 0) {  // 8.2 the violet highlights (the 30 u layer only); none in Thermal / night vision
        g_dpHiRgb = kDpHiCol, g_dpHiGain = DP_HI_GAIN;
        DpStates(dev, tex, 1);
        tris += DpDisc(dev, vp, g_dpCen[0], g_dpCen[1] - kDpLayer[1].h, g_dpCen[2], rmax, DP_RINGS, DP_SECTS, &kDpLayer[1], 1.0f, 1, 1,
                       2.1f, 0.9f);
    }
    g_dpHoleOn = 0;
    return tris;
}
static void DpRoomLog(DWORD room, int tris, float R, const Viewport8 *vp, const char *where)  // vp NULL: a failed draw (where)
{
    static DWORD s_room = 0xFFFFFFFFu;
    static float s_t0;
    int k;
    if (room != s_room) {
        s_room = room, s_t0 = g_dpNow, g_dpSLDrop = g_dpRibDrop = 0;  // (g_dpMsMax: reset in DpFrameBegin, before the first timed frame)
    } else if (s_t0 >= 0.0f && g_dpNow - s_t0 > 3.0f) {  // v3.27.1 (user item 5): once per room, what the fog did there
        static const char *const kLook[4] = { "full", "Thermal", "night vision", "Spirit Vision" };
        float mn = 9.0e9f, sum = 0.0f;
        for (k = 0; k < DP_WALLS; ++k) {
            mn = g_dpWall[k] < mn ? g_dpWall[k] : mn;
            sum += g_dpWall[k] < 5000.0f ? g_dpWall[k] : 5000.0f;
        }
        s_t0 = -1.0f;
        Log("death presence: stage %lX room %lX (%s, look %s): %d triangles, %d sprites a frame (%d over the list, %d ribbons over theirs), her model "
            "drawn %s, %d being(s) shown, walls min %.0f mean %.0f (R %.0f), viewport %s, eye %s, resource misses %d, the "
            "slowest frame %.2f ms; depth: %s, projection %s (m22 %g m32 %g; per-model %g %g), near %g far %g, camera %.0f from her",
            room >> 16, room & 0xFFFF, where, kLook[g_dpLook & 3], tris, g_dpSLDrawn, g_dpSLDrop, g_dpRibDrop,
            g_dpHerOk ? "yes" : "no", g_dpVictimsDrawn, mn, sum / DP_WALLS, R, !vp ? "none" : vp == &g_dpVp ? "3D" : "saved",
            g_svEyeOk ? "ok" : "none", g_dpDrawMissTotal, g_dpMsMax, g_dpZExact ? "exact" : "x 1/128 (v3.27.2)", g_dpProjSrc == 1 ? "camera" : g_dpProjSrc == 2 ? "per-model" : "none",
            g_dpProjUsed[0], g_dpProjUsed[1], g_dpProjOther[0], g_dpProjOther[1], *(volatile float *)0x12E3E7C,
            *(volatile float *)0x12E3E80, g_svEyeOk ? sqrtf((g_svEye[0] - g_dpCen[0]) * (g_svEye[0] - g_dpCen[0]) +
            (g_svEye[1] - g_dpCen[1]) * (g_svEye[1] - g_dpCen[1]) + (g_svEye[2] - g_dpCen[2]) * (g_svEye[2] - g_dpCen[2])) : -1.0f);
    }
    g_dpSLDrawn = 0;
}
static float DpMsSince(const LARGE_INTEGER *t0)
{
    LARGE_INTEGER t1, f;
    QueryPerformanceCounter(&t1);
    return QueryPerformanceFrequency(&f) && f.QuadPart ? (float)((double)(t1.QuadPart - t0->QuadPart) * 1000.0 / (double)f.QuadPart) : 0.0f;
}
static void DpDrawC(void)
{
    static int s_logged;
    const int level = (int)g_dpLevel > 0 && (int)g_dpLevel < DP_LEVELS ? (int)g_dpLevel : 1;
    const float R = kDpLevel[level].R;
    void *dev = DEV, *tex;
    const Viewport8 *vp;
    FxSaved sv;
    float pp[3];
    int tris = 0;
    DWORD room;
    LARGE_INTEGER t0;
    QueryPerformanceCounter(&t0);
    if (!dev || !FxOnBackBuffer(dev)) {  // v3.27.1: logged once per room (a "vanished" report)
        static DWORD s_bbRoom = 0xFFFFFFFFu;
        const DWORD rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
        DpClock();
        if (rk != s_bbRoom) {
            s_bbRoom = rk;
            Log("death presence: stage %lX room %lX: the scene is not drawn to the back buffer here: no fog", rk >> 16, rk & 0xFFFF);
        }
        return;
    }
    g_dpLook = DpLookNow();
    if (!DpFrameBegin(pp, &room, R)) return;
    if (g_dpLook >= 1) {  // Spirit Vision / Thermal / night vision (v3.27.2): nothing before the filter; GvPass / FxPass call
        // DpPostDraw after their own passes (PLAN 5.1; v3.27.2 PLAN 5.2 / 5.3)
        // review DP3271-003 / R2-003: a request GvPass did not serve last frame (its shaders failed: it draws nothing of
        // Spirit Vision) = the full dark look for the rest of this room (a new room asks again; a room change starts with
        // its fade), so the fog does not vanish while it still hurts
        // review DP3272-002: per look: only an unserved request OF THE SAME LOOK and visit marks that look broken; another
        // look in the same room still asks its own pass
        if (g_dpSvFrame == g_fxFrame - 1 && g_dpSvDone != g_fxFrame - 1 && g_dpSvAskVisit == g_dpVisit && g_dpPostLook == g_dpLook &&
            g_dpSvBroken[g_dpLook & 3] != g_dpVisit) {
            g_dpSvBroken[g_dpLook & 3] = g_dpVisit;  // only a request of this same visit counts (review DP3271-R3-001)
            Log("death presence: stage %lX room %lX: %s did not draw the fog's look: the dark look in this room",
                room >> 16, room & 0xFFFF, g_dpLook == 3 ? "Spirit Vision" : g_dpLook == 1 ? "Thermal" : "night vision");
        }
        if (g_dpSvBroken[g_dpLook & 3] != g_dpVisit) {
            g_dpSvFrame = g_fxFrame;
            g_dpSvAskVisit = g_dpVisit;
            g_dpPostLook = g_dpLook;
            g_dpMsPart = DpMsSince(&t0), g_dpMsPartFrame = g_fxFrame;
            return;
        }
        // the fallback before the filter: Spirit Vision the full look; Thermal its dark look (colours x 0.08, no additive
        // light) and night vision its own (review DP3272-REV-001: Thermal had fallen back to the full violet look)
        g_dpLook = g_dpLook == 3 ? 0 : g_dpLook;
    }
    // review DP-I202: no fog = no effect: resources that stay unavailable for 300 consecutive frames (a device reset
    // recovers well within that) turn the feature off (fail-closed); a success clears the count
    {
        const char *miss = (tex = FxTexture(dev, FXX_FOG)) == NULL ? "texture fog" : !FxTexture(dev, FXX_WISP) ? "texture wisps" :  // DP-I303
                           !FxTexture(dev, FXX_SVATLAS) ? "texture atlas" : !FxTexture(dev, FXX_SMOKE) ? "texture smoke" :  // v3.27.1
                           !FxSave(dev, &sv, 0) ? "device state" : NULL;
        if (miss) {
            DpMissLog(room, miss);  // review DP3271-004 / R2-001: named at once
            ++g_dpDrawMissTotal;
            DpRoomLog(room, 0, R, NULL, miss);  // (and in the room's line, when it is written)
            if (++g_dpDrawMiss >= 300) DpFailNow("the fog's resources (texture or device state)", 0, NULL);
            return;
        }
    }
    g_dpDrawMiss = 0;
    BG_RUN(BGC_DFX, DepthFixFrame(dev, NULL, DFW_DP, g_dpLook));  // v3.31 DEPTH331: her true depth first (depthfix.inc); v3.35 FX335: its own ledger part
    vp = g_dpVpOk && g_dpMdlStamp == g_fxFrame ? &g_dpVp : &sv.vp;  // the 3D viewport the world was drawn in
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv.vpSet = 1;
    __try {  // whatever happens below, the device states go back (the fault itself is caught by DpDrawFrame)
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
        SvNearFar();
        DpCamProj();  // v3.27.2 (PLAN 2.2)
        g_svEyeOk = SvEye(g_svEye);
        FxBase(dev);
        SvZ(dev, 2);  // the visible part only (reversed Z: GREATEREQUAL), no z write
        tris += DpPool(dev, vp, tex, R, level, pp);
        tris += DpSprFrame(dev, vp, R, level, pp);  // the volume, wisps, her aura, victims, Room 302 columns
        if (g_dpProbe) tris += DpProbe(dev, vp, pp);  // v3.27.2 (PLAN 2.4): hidden depth probe
    } __finally {
        g_svZScaleNow = SV_ZSCALE;  // v3.27.3: Spirit Vision keeps its own calibration
        FxRestore(dev, &sv);
    }
    {
        const float ms = DpMsSince(&t0);
        if (ms > g_dpMsMax) g_dpMsMax = ms;
    }
    DpRoomLog(room, tris, R, vp, "scene pass");
    if (!s_logged++)
        Log("death presence: the fog draws (%d triangles in its first frame, radius %.0f, %d being(s) shown, look %d)", tris, R,
            g_dpArtN, g_dpLook);
}
// v3.27.1 (PLAN 5.1) / v3.27.2 (PLAN 5.2): after the filter. Spirit Vision: called by GvPass in its gameplay block, after
// the lift / edge / grade passes and its own auras: the fog as violet-blue spirit light. Thermal: called by FxPass after
// ThHeatPass: the dark look in black over the heat palette. vp NULL = the 3D viewport captured at 0x52AEB5 (else the
// current one). Its own FxSave / FxRestore, matrix, near / far, camera projection.
static void DpPostDraw(void *dev, const Viewport8 *vpIn)
{
    void *tex;
    float pp[3];
    int tris = 0, saved = 0;
    const Viewport8 *vp;
    FxSaved sv;
    LARGE_INTEGER t0;
    const int look = g_dpPostLook;
    if (!g_dpInstalled || g_dpFail || g_dpLevel <= 0 || g_dpSvFrame != g_fxFrame || !dev) return;
    if ((look == 3) != (vpIn != NULL)) return;  // the request's look must match the caller (GvPass passes a viewport)
    QueryPerformanceCounter(&t0);
    __try {
        const int level = (int)g_dpLevel < DP_LEVELS ? (int)g_dpLevel : 1;
        const float R = kDpLevel[level].R;
        static const float kSvCol[3] = { 0.30f, 0.30f, 0.95f };
        {   // review DP3271-003: the same miss accounting and fail-closed rule as the scene pass
            const char *miss = (tex = FxTexture(dev, FXX_FOG)) == NULL ? "texture fog" : !FxTexture(dev, FXX_SMOKE) ? "texture smoke" :
                               !FxTexture(dev, FXX_SVATLAS) ? "texture atlas" : !FxTexture(dev, FXX_WISP) ? "texture wisps" :
                               !FxSave(dev, &sv, 0) ? "device state" : NULL;
            if (miss) {  // review DP3271-IR-001: not served: the scene pass draws the dark look from the next frame
                DpMissLog(g_dpDrawRoom, miss);
                ++g_dpDrawMissTotal;
                DpRoomLog(g_dpDrawRoom, 0, R, NULL, miss);
                if (++g_dpDrawMiss >= 300) DpFailNow("the fog's resources (texture or device state)", 0, NULL);
                return;
            }
            saved = 1;
            g_dpDrawMiss = 0;
            g_dpSvDone = g_fxFrame;  // served (the textures are there)
        }
        BG_RUN(BGC_DFX, DepthFixFrame(dev, NULL, DFW_DP, look));  // v3.31 DEPTH331: her true depth first (depthfix.inc); v3.35 FX335: ledger
        memcpy(pp, (const void *)0x10A0F60, 12);
        if (!DpFinite3(pp)) __leave;
        vp = vpIn ? vpIn : g_dpVpOk && g_dpMdlStamp == g_fxFrame ? &g_dpVp : &sv.vp;
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
        sv.vpSet = 1;
        ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
        SvNearFar();
        DpCamProj();
        g_svEyeOk = SvEye(g_svEye);
        FxBase(dev);
        SvZ(dev, 2);
        g_dpLook = look;
        if (look == 3) {
            g_dpHoleOn = 0;  // the floor pool as an additive glow (v3.27.2: gains 0.060 / 0.036, PLAN 5.1)
            g_dpHiRgb = kSvCol;
            DpStates(dev, tex, 1);
            g_dpHiGain = 0.080f;  // v3.27.3 (v3.27.2 0.060)
            tris += DpDisc(dev, vp, g_dpCen[0], g_dpCen[1] - kDpLayer[0].h, g_dpCen[2], DP_MASK_OUT * R, DP_RINGS, DP_SECTS, &kDpLayer[0],
                           1.0f, 1, 1, 0.0f, 0.9f);
            g_dpHiGain = 0.048f;  // v3.27.3 (v3.27.2 0.036)
            tris += DpDisc(dev, vp, g_dpCen[0], g_dpCen[1] - kDpLayer[1].h, g_dpCen[2], DP_MASK_OUT * R, DP_RINGS, DP_SECTS, &kDpLayer[1],
                           1.0f, 1, 1, 2.1f, 0.9f);
            g_dpHiRgb = kDpHiCol, g_dpHiGain = DP_HI_GAIN;
        } else {
            tris += DpPool(dev, vp, tex, R, level, pp);  // Thermal / night vision: the dark pool, black (DpEmit)
        }
        tris += DpSprFrame(dev, vp, R, level, pp);
        if (g_dpProbe) tris += DpProbe(dev, vp, pp);
        {   // review DP3271-I03: the whole fog frame = this pass + the scene pass's part of the same frame
            const float ms = DpMsSince(&t0) + (g_dpMsPartFrame == g_fxFrame ? g_dpMsPart : 0.0f);
            if (ms > g_dpMsMax) g_dpMsMax = ms;
        }
        DpRoomLog(g_dpDrawRoom, tris, R, vp, look == 3 ? "Spirit Vision pass" : look == 1 ? "Thermal pass" : "night-vision pass");
    } __except (DpFaultNote(GetExceptionInformation())) {
        g_dpHiRgb = kDpHiCol, g_dpHiGain = DP_HI_GAIN, g_dpHoleOn = 0;
        DpFailNow("the post-filter fog", g_dpFaultCode, g_dpFaultAt);
    }
    g_svZScaleNow = SV_ZSCALE;  // v3.27.3: Spirit Vision keeps its own calibration
    if (saved) FxRestore(dev, &sv);
}
static void DpDrawFrame(void)
{
    if (!g_dpInstalled || g_dpFail || g_dpLevel <= 0 || !g_fxOn) return;
    if (*(volatile int *)0xFD5C20 != 0 || !g_spInstalled) return;  // gameplay frames (the capture frame included: phase 0)
    if (!FxGameplay()) return;
    if (DpOffNow()) return;  // v3.42 SM342: off in Room 302 (no fog; DpPostDraw is asked by DpDrawC only)
    __try {
        DpDrawC();
    } __except (DpFaultNote(GetExceptionInformation())) {
        DpFailNow("the fog draw", g_dpFaultCode, g_dpFaultAt);
    }
}
