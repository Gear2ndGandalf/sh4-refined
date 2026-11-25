// ================================================================ v3.3 [Items] Sparkle: drawing (items.inc uses it)
//
// A small additive star drawn with the game's own IDirect3DDevice8 ([0x6E7D94]) as pre-transformed quads (XYZRHW,
// DIFFUSE, TEX1 = FVF 0x144) after the world, with our own 64x64 A8R8G8B8 texture in the managed pool (made once per
// device, so it survives Reset). Every state it touches is read first and put back, the vertex and pixel shaders too.
// D3D8 vtable indices (d3d8.h order): CreateTexture 20, GetViewport 41, SetRenderState 50, GetRenderState 51,
// GetTexture 60, SetTexture 61, GetTextureStageState 62, SetTextureStageState 63, DrawPrimitiveUP 72,
// SetVertexShader 76, GetVertexShader 77, SetStreamSource 83, GetStreamSource 84, SetPixelShader 88, GetPixelShader 89;
// texture LockRect 16, UnlockRect 17. (v3.3.1: v3.3.0 called 84 / 85 as the pixel shader pair; 85 is SetIndices, and
// handing it a stack address corrupted the game's vertex buffers: the user's crash at 0x415F0B, a Lock on a freed VB.)
// DrawPrimitiveUP sets stream 0 to NULL (D3D8), so the game's stream 0 is read first and put back too.

typedef struct { float x, y, z, size, alpha, spin; } SpPt;  // centre in render-target pixels, depth 0..1, size in
                                                             // pixels, alpha 0..1

typedef HRESULT(STDMETHODCALLTYPE *SpCreateTex_t)(void *, UINT, UINT, UINT, DWORD, DWORD, DWORD, void **);
typedef HRESULT(STDMETHODCALLTYPE *SpLock_t)(void *, UINT, void *, const RECT *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *SpUnlock_t)(void *, UINT);
typedef HRESULT(STDMETHODCALLTYPE *SpSetRS_t)(void *, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *SpGetRS_t)(void *, DWORD, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *SpGetTex_t)(void *, DWORD, void **);
typedef HRESULT(STDMETHODCALLTYPE *SpSetTex_t)(void *, DWORD, void *);
typedef HRESULT(STDMETHODCALLTYPE *SpSetTSS_t)(void *, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *SpGetTSS_t)(void *, DWORD, DWORD, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *SpDrawUP_t)(void *, DWORD, UINT, const void *, UINT);
typedef HRESULT(STDMETHODCALLTYPE *SpSetDW_t)(void *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *SpGetDW_t)(void *, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *SpGetStream_t)(void *, UINT, void **, UINT *);
typedef HRESULT(STDMETHODCALLTYPE *SpSetStream_t)(void *, UINT, void *, UINT);

enum { SPV_CREATETEX = 20, SPV_GETVP = 41, SPV_SETRS = 50, SPV_GETRS = 51, SPV_GETTEX = 60, SPV_SETTEX = 61,
       SPV_GETTSS = 62, SPV_SETTSS = 63, SPV_DRAWUP = 72, SPV_SETVS = 76, SPV_GETVS = 77, SPV_SETSTREAM = 83, SPV_GETSTREAM = 84,
       SPV_SETPS = 88, SPV_GETPS = 89 };

static void *g_spTex, *g_spTexDev;  // our texture and the device it belongs to
static int g_spTexFailed;

#define SP_TEX 64
// The picture: a soft round glow plus a four-point star (long thin rays on the axes, short ones on the diagonals),
// white, the shape in alpha (premultiplied into the colour too, since the blend is additive: ONE, ONE).
static int SparkleMakeTex(void *dev)
{
    struct { int pitch; BYTE *bits; } lr;
    void *tex = NULL;
    int x, y;
    if (FAILED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, SP_TEX, SP_TEX, 1, 0, 21 /*A8R8G8B8*/, 1 /*MANAGED*/, &tex)) || !tex)
        return 0;
    if (FAILED(((SpLock_t)VT(tex, 16))(tex, 0, &lr, NULL, 0))) {
        ((Release_t)VT(tex, VT_RELEASE))(tex);
        return 0;
    }
    for (y = 0; y < SP_TEX; ++y) {
        DWORD *row = (DWORD *)(lr.bits + y * lr.pitch);
        for (x = 0; x < SP_TEX; ++x) {
            const float u = (x + 0.5f) / (SP_TEX * 0.5f) - 1.0f, v = (y + 0.5f) / (SP_TEX * 0.5f) - 1.0f;
            const float r = sqrtf(u * u + v * v);
            const float au = fabsf(u), av = fabsf(v);
            const float d1 = fabsf(u - v) * 0.70710678f, d2 = fabsf(u + v) * 0.70710678f;  // distance to the diagonals
            float a = 0.0f, ray;
            // glow: bright core, soft halo
            a += expf(-r * r * 60.0f) * 1.0f + expf(-r * r * 9.0f) * 0.35f;
            // axis rays: thin, fading towards the edge
            ray = expf(-av * av * 900.0f) * (1.0f - au) + expf(-au * au * 900.0f) * (1.0f - av);
            if (ray > 0.0f) a += ray * 0.9f;
            // diagonal rays: thinner and half as long
            ray = expf(-d1 * d1 * 1400.0f) + expf(-d2 * d2 * 1400.0f);
            if (r < 0.5f) a += ray * (1.0f - r * 2.0f) * 0.55f;
            if (r > 0.98f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            {
                const DWORD c = (DWORD)(a * 255.0f + 0.5f);
                row[x] = c << 24 | c << 16 | c << 8 | c;
            }
        }
    }
    ((SpUnlock_t)VT(tex, 17))(tex, 0);
    g_spTex = tex;
    g_spTexDev = dev;
    return 1;
}

typedef struct { float x, y, z, rhw; DWORD c; float u, v; } SpVtx;

// Draws the points on the current render target, inside the game's scene (between its BeginScene and EndScene).
// tint: 0xRRGGBB of the sparkle (alpha comes from each point).
// v3.3.2: depth = 1 tests each point against the game's depth buffer (ZENABLE on, LESSEQUAL, no writes), so Eileen,
// enemies and props in front hide it pixel by pixel.
static void SparkleDrawList(const SpPt *pts, int n, DWORD tint, int depth)
{
    static const DWORD kRs[] = { 7 /*ZENABLE*/, 14 /*ZWRITEENABLE*/, 15 /*ALPHATESTENABLE*/, 19 /*SRCBLEND*/,
                                 20 /*DESTBLEND*/, 22 /*CULLMODE*/, 27 /*ALPHABLENDENABLE*/, 28 /*FOGENABLE*/,
                                 52 /*STENCILENABLE*/, 137 /*LIGHTING*/, 168 /*COLORWRITEENABLE*/, 171 /*BLENDOP*/,
                                 23 /*ZFUNC*/ };
    DWORD kRsSet[] = { 0, 0, 0, 2 /*ONE*/, 2 /*ONE*/, 1 /*NONE*/, 1, 0, 0, 0, 0xF, 1 /*ADD*/, 4 /*LESSEQUAL*/ };
    static const DWORD kTss[][2] = { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 }, { 0, 6 }, { 0, 11 }, { 0, 13 },
                                     { 0, 14 }, { 0, 16 }, { 0, 17 }, { 0, 18 }, { 0, 24 }, { 1, 1 }, { 1, 4 } };
    static const DWORD kTssSet[] = { 4 /*MODULATE*/, 2 /*TEXTURE*/, 0 /*DIFFUSE*/, 4 /*MODULATE*/, 2, 0, 0, 3 /*CLAMP*/,
                                     3, 2 /*LINEAR*/, 2, 0 /*NONE*/, 0, 1 /*DISABLE*/, 1 };
    DWORD rs[sizeof kRs / sizeof kRs[0]], tss[sizeof kTssSet / sizeof kTssSet[0]], vs = 0, ps = 0;
    void *dev = DEV, *oldTex = NULL, *oldVb = NULL;
    UINT oldStride = 0;
    SpVtx v[6 * 16];
    int i, k, m;
    if (!dev || n <= 0) return;
    kRsSet[0] = depth ? 1 : 0;  // ZENABLE (D3DZB_TRUE)
    if (dev != g_spTexDev) {  // a new device: the old texture went with it (not released: it was the old device's)
        g_spTex = NULL;
        g_spTexDev = NULL;
        if (g_spTexFailed > 3) return;
        if (!SparkleMakeTex(dev)) {
            if (++g_spTexFailed == 1) Log("items: the shine texture could not be made; no item shine");
            return;
        }
    }
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]);
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        ((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, kTss[i][0], kTss[i][1], &tss[i]);
    ((SpGetDW_t)VT(dev, SPV_GETVS))(dev, &vs);
    ((SpGetDW_t)VT(dev, SPV_GETPS))(dev, &ps);
    ((SpGetTex_t)VT(dev, SPV_GETTEX))(dev, 0, &oldTex);  // AddRef'd
    if (FAILED(((SpGetStream_t)VT(dev, SPV_GETSTREAM))(dev, 0, &oldVb, &oldStride))) oldVb = NULL;  // AddRef'd

    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], kRsSet[i]);
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kTss[i][0], kTss[i][1], kTssSet[i]);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);  // FVF XYZRHW | DIFFUSE | TEX1
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, g_spTex);

    for (i = 0; i < n; i += 16) {
        const int cnt = n - i < 16 ? n - i : 16;
        for (k = 0; k < cnt; ++k) {
            const SpPt *p = &pts[i + k];
            const float h = p->size * 0.5f, cs = cosf(p->spin) * h, sn = sinf(p->spin) * h;
            float a = p->alpha;
            DWORD c;
            SpVtx q[4];
            if (a < 0.0f) a = 0.0f;
            if (a > 1.0f) a = 1.0f;
            // additive: the colour carries the strength
            c = (DWORD)(((tint >> 16) & 0xFF) * a) << 16 | (DWORD)(((tint >> 8) & 0xFF) * a) << 8 | (DWORD)((tint & 0xFF) * a) |
                0xFF000000u;
            for (m = 0; m < 4; ++m) {
                const float cx = (m == 1 || m == 2) ? 1.0f : -1.0f, cy = (m >= 2) ? 1.0f : -1.0f;
                q[m].x = p->x + cx * cs - cy * sn - 0.5f;
                q[m].y = p->y + cx * sn + cy * cs - 0.5f;
                q[m].z = depth ? p->z : 0.0f;
                q[m].rhw = 1.0f;
                q[m].c = c;
                q[m].u = cx > 0.0f ? 1.0f : 0.0f;
                q[m].v = cy > 0.0f ? 1.0f : 0.0f;
            }
            v[k * 6 + 0] = q[0];
            v[k * 6 + 1] = q[1];
            v[k * 6 + 2] = q[2];
            v[k * 6 + 3] = q[0];
            v[k * 6 + 4] = q[2];
            v[k * 6 + 5] = q[3];
        }
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(cnt * 2), v, sizeof(SpVtx));
    }

    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, oldTex);
    if (oldVb) {
        ((SpSetStream_t)VT(dev, SPV_SETSTREAM))(dev, 0, oldVb, oldStride);
        ((Release_t)VT(oldVb, VT_RELEASE))(oldVb);
    }
    if (oldTex) ((Release_t)VT(oldTex, VT_RELEASE))(oldTex);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, vs);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, ps);
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, kTss[i][0], kTss[i][1], tss[i]);
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
}
