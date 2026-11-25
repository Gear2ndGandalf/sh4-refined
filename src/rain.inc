// ================================================================ v3.26.0 Noise Type Rain on glass: the draw
// staging/v326_sv_rain/PLAN.md section 3 (Codex plan review, findings SVR-001..203). The simulation is rain_sim.h
// (fixed 1/120 s steps); this file advances it on the frame's real time and draws it, from FxNoiseLayer (type 6) inside
// the game's noise draw (0x432FBE). Two states: the gameplay rain and the Screen Effects page preview (SVR-201).
// Per frame, all inside FxNoiseLayer's FxSave / FxRestore:
//   1. a checked copy of the back buffer into the game's frame-copy texture [0x13DC3A0] (SVR-001: 0x4036E0 ignores
//      the CopyRects result, so Refined copies itself and draws the refraction only after SUCCEEDED);
//   2. refraction: drops, beads, micro-drops and trail segments sample that copy (drops: inverted, minified 2.4 x, the
//      wide-angle look of a drop on a window; trails: shifted 3 px across), shaped by the alpha of an atlas on stage 1;
//      SRCALPHA / INVSRCALPHA, no pixel shader;
//   3. the 2x-modulate layer (FxOverlayStates, the Noise Colour shader when one is set): dark rims, glints, caustics,
//      the trails' light lines; with a Noise Colour the per-quad strengths go in 8 buckets (the shader's strength is a
//      constant);
//   4. the v3.18.0 falling streaks, fewer and fainter.
// Every quad goes through RnPush*, which draws the array when it is full and continues (SVR-003: nothing dropped).
#include "rain_sim.h"

#define RN_M           2.4f     // the drop shows a region this many times its size (inverted: minified)
#define RN_LOOK_UP     0.3f     // ... centred this x r above it
#define RN_REFR_DIFF   0.95f    // the refraction slightly darker than the scene
#define RN_TRAIL_SHIFT 6.0f     // a trail's film shifts the scene this many px across it
#define RN_TRAIL_REFR  0.80f    // ... at this opacity
#define RN_VMAX        3072     // vertices per draw array (flushed when full)
#define RN_GLINT       0.24f       // the additive glint (renders: the 2x-modulate layer cannot light a black scene)

typedef struct { float x, y, z, rhw; DWORD c; float u0, v0, u1, v1; } RnV2;  // FVF XYZRHW | DIFFUSE | TEX2 (0x244)
static RnV2 g_rnV2[RN_VMAX];
static SpVtx g_rnV1[RN_VMAX];
static int g_rnN2, g_rnN1, g_rnDraws;
static RnState g_rn[2];          // 0 gameplay, 1 the Screen Effects page preview
static int g_rnInit[2];
static LONG g_rnFrame[2] = { -100, -100 };
static LONGLONG g_rnLast[2];     // the clock (FxClock ticks) at this state's last frame
static FxStreak g_rnStreak[2][3];  // INSP326-008: the falling streaks per state, on the state's own rain clock
static float g_rnUnit, g_rnTw, g_rnTh;  // px per 720-line px; the frame-copy surface size
static float g_rnGlintRgb[3];           // the glint colour (white, or the Noise Colour's mask)

static void RnFlush2(void *dev)
{
    if (g_rnN2 >= 3) {
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(g_rnN2 / 3), g_rnV2, sizeof(RnV2));
        ++g_rnDraws;
    }
    g_rnN2 = 0;
}
static void RnFlush1(void *dev)
{
    if (g_rnN1 >= 3) {
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(g_rnN1 / 3), g_rnV1, sizeof(SpVtx));
        ++g_rnDraws;
    }
    g_rnN1 = 0;
}
// a quad (corners 0 1 2 3 = top-left, top-right, bottom-left, bottom-right) with two UV sets
static void RnPush2(void *dev, const float x[4], const float y[4], const float u0[4], const float v0[4], const float u1[4],
                    const float v1[4], DWORD c)
{
    static const int kIdx[6] = { 0, 1, 2, 1, 3, 2 };
    int i;
    if (g_rnN2 + 6 > RN_VMAX) RnFlush2(dev);
    for (i = 0; i < 6; ++i) {
        RnV2 *o = &g_rnV2[g_rnN2++];
        const int k = kIdx[i];
        o->x = x[k] - 0.5f, o->y = y[k] - 0.5f, o->z = 0.0f, o->rhw = 1.0f, o->c = c;
        o->u0 = u0[k], o->v0 = v0[k], o->u1 = u1[k], o->v1 = v1[k];
    }
}
static void RnPush1(void *dev, const float x[4], const float y[4], const float u[4], const float v[4], DWORD c)
{
    static const int kIdx[6] = { 0, 1, 2, 1, 3, 2 };
    int i;
    if (g_rnN1 + 6 > RN_VMAX) RnFlush1(dev);
    for (i = 0; i < 6; ++i) {
        SpVtx *o = &g_rnV1[g_rnN1++];
        const int k = kIdx[i];
        o->x = x[k] - 0.5f, o->y = y[k] - 0.5f, o->z = 0.0f, o->rhw = 1.0f, o->c = c;
        o->u = u[k], o->v = v[k];
    }
}
// the drop atlas cell's UVs (2 x 2 cells of 128 texels, half a texel in)
static void RnCell(int cell, float u[4], float v[4])
{
    const float u0 = 0.5f * (float)(cell & 1) + 0.5f / 256.0f, v0 = 0.5f * (float)((cell >> 1) & 1) + 0.5f / 256.0f;
    const float u1 = u0 + 0.5f - 1.0f / 256.0f, v1 = v0 + 0.5f - 1.0f / 256.0f;
    u[0] = u[2] = u0, u[1] = u[3] = u1;
    v[0] = v[1] = v0, v[2] = v[3] = v1;
}
static DWORD RnGreyA(float a, float grey) { return (DWORD)(FxSat(a) * 255.0f + 0.5f) << 24 | (DWORD)(FxSat(grey) * 255.0f + 0.5f) * 0x010101u; }

// ---- what is drawn: one walker over the state, called once per pass. mode 0 refraction (drops, beads, mist; stage 1
// = the drop atlas), 1 refraction trails (stage 1 = the trail texture), 2 modulate drops, 3 modulate trails, 4 additive
// glints (FXX_GLOW, ONE / ONE). bucket < 0:
// every quad; 0..7: only the quads of that strength bucket (Noise Colour), drawn at the bucket's strength.
static void RnWalk(void *dev, const RnState *s, int mode, float kd, int bucket)
{
    const float U = g_rnUnit;
    float x[4], y[4], u[4], v[4], fu[4], fv[4];
    int i, k;
    if (mode == 0 || mode == 2 || mode == 4) {
        for (i = 0; i < RN_DROPS + RN_BEADS + RN_MIST; ++i) {
            float cx, cy, r, fade, sy = 1.0f;
            int cell;
            if (i < RN_DROPS) {  // drops
                const RnDrop *d = &s->d[i];
                const float grow = 0.4f + 0.6f * FxSmooth(0.0f, RN_GROW_S * RN_HZ, (float)d->age);
                if (!d->st) continue;
                cx = d->x, cy = d->y, r = d->r * (d->st == 1 && !d->bead ? grow : 1.0f), cell = d->shape;  // INSP326-014: the impact growth only
                                                                                                      // for a new drop (fading / bead ages restart)
                fade = d->st == 3 ? 1.0f - (float)d->age / (RN_LIFE_FADE * RN_HZ) : 1.0f;
                if (d->st == 2) sy = 1.0f + 0.15f * FxSat(d->spd / 120.0f);  // a moving drop stretches along its path
            } else if (i < RN_DROPS + RN_BEADS) {  // beads
                const RnBead *b = &s->bead[i - RN_DROPS];
                const int age = s->step - b->born;
                if (b->life <= 0 || age < 0 || age >= b->life) continue;
                cx = b->x, cy = b->y, r = b->r, cell = (int)(b->seed >> 8) & 3;
                fade = FxSmooth(0.0f, 0.15f * RN_HZ, (float)age) * (1.0f - FxSmooth((float)b->life - 0.5f * RN_HZ, (float)b->life, (float)age));
            } else {  // micro-drops
                const int j = i - RN_DROPS - RN_BEADS;
                const RnMist *m = &s->mist[j];
                RnMistPos(s, j, &cx, &cy, &r);
                cell = j & 3;
                fade = m->cleared >= 0 ? 1.0f - (float)(s->step - m->cleared) / (RN_MIST_CLEAR * RN_HZ)
                                       : FxSmooth(0.0f, RN_MIST_FILL * RN_HZ, (float)(s->step - m->appear));
                fade *= 0.8f;
            }
            if (!(fade > 0.01f) || !(r > 0.3f)) continue;
            {
                const float X = cx * U, Y = cy * U, R = r * U, Ry = R * sy;
                x[0] = x[2] = X - R, x[1] = x[3] = X + R;
                y[0] = y[1] = Y - Ry, y[2] = y[3] = Y + Ry;
                RnCell(cell, u, v);
                if (mode == 0) {  // inverted: the left edge samples the right, the top the bottom (SVR-004)
                    const float Yc = Y - RN_LOOK_UP * R, H = RN_M * R;
                    fu[0] = fu[2] = (X + H) / g_rnTw, fu[1] = fu[3] = (X - H) / g_rnTw;
                    fv[0] = fv[1] = (Yc + H) / g_rnTh, fv[2] = fv[3] = (Yc - H) / g_rnTh;
                    RnPush2(dev, x, y, fu, fv, u, v, RnGreyA((0.55f + 0.45f * kd) * fade, RN_REFR_DIFF));
                } else if (mode == 4) {  // an additive glint upper-left (lights the drop in a dark room)
                    const float gx = X - 0.34f * R, gy = Y - 0.40f * Ry, gr = 0.30f * R + 0.6f * U, gk = RN_GLINT * (0.5f + 0.5f * kd) * fade;
                    const DWORD gc = (DWORD)(FxSat(gk * g_rnGlintRgb[0]) * 255.0f + 0.5f) << 16 | (DWORD)(FxSat(gk * g_rnGlintRgb[1]) * 255.0f + 0.5f) << 8 |
                                     (DWORD)(FxSat(gk * g_rnGlintRgb[2]) * 255.0f + 0.5f) | 0xFF000000u;
                    if (i >= RN_DROPS + RN_BEADS) continue;  // not the micro-drops
                    x[0] = x[2] = gx - gr, x[1] = x[3] = gx + gr;
                    y[0] = y[1] = gy - gr, y[2] = y[3] = gy + gr;
                    u[0] = u[2] = 0.0f, u[1] = u[3] = 1.0f, v[0] = v[1] = 0.0f, v[2] = v[3] = 1.0f;
                    RnPush1(dev, x, y, u, v, gc);
                } else {
                    const float a = kd * fade;
                    if (bucket >= 0) {
                        k = (int)(FxSat(a) * 8.0f);
                        if ((k > 7 ? 7 : k) != bucket) continue;
                    }
                    RnPush1(dev, x, y, u, v, FxAlpha(a));
                }
            }
        }
        return;
    }
    for (i = 0; i < RN_SEGS; ++i) {  // trail segments: thinning from their own birth, oldest (top) first
        const RnSeg *g = &s->seg[i];
        const int age = s->step - g->born;
        float dx, dy, len, nx, ny, f, hw, ex, ey;
        if (g->life <= 0 || age < 0 || age >= g->life) continue;
        f = 1.0f - (float)age / (float)g->life;
        hw = 0.5f * g->w * powf(f, 0.7f) * U;
        if (!(hw > 0.15f)) continue;
        dx = (g->x1 - g->x0) * U, dy = (g->y1 - g->y0) * U;
        len = sqrtf(dx * dx + dy * dy);
        if (!(len > 0.01f)) continue;
        nx = -dy / len, ny = dx / len;
        ex = dx / len * 0.5f, ey = dy / len * 0.5f;  // ends overlap by half a pixel
        x[0] = g->x0 * U - ex + nx * hw, y[0] = g->y0 * U - ey + ny * hw;
        x[1] = g->x0 * U - ex - nx * hw, y[1] = g->y0 * U - ey - ny * hw;
        x[2] = g->x1 * U + ex + nx * hw, y[2] = g->y1 * U + ey + ny * hw;
        x[3] = g->x1 * U + ex - nx * hw, y[3] = g->y1 * U + ey - ny * hw;
        u[0] = u[2] = 0.0f, u[1] = u[3] = 1.0f;  // across the trail texture
        v[0] = v[1] = 0.25f, v[2] = v[3] = 0.75f;
        if (mode == 1) {
            const float sh = RN_TRAIL_SHIFT * U;
            for (k = 0; k < 4; ++k) fu[k] = (x[k] + nx * sh) / g_rnTw, fv[k] = (y[k] + ny * sh) / g_rnTh;
            RnPush2(dev, x, y, fu, fv, u, v, RnGreyA(RN_TRAIL_REFR * f * (0.6f + 0.4f * kd), 1.0f));
        } else {
            const float a = kd * 0.8f * f;
            if (bucket >= 0) {
                k = (int)(FxSat(a) * 8.0f);
                if ((k > 7 ? 7 : k) != bucket) continue;
            }
            RnPush1(dev, x, y, u, v, FxAlpha(a));
        }
    }
}
// SVR-001: Refined's own checked copy of the back buffer into the game's frame-copy texture. 1 = the copy succeeded
// (g_rnTw / g_rnTh = the surface size for the UVs); every reference taken is released.
static int RnCopy(void *dev, void **texOut)
{
    void *tex = *(void *volatile *)0x13DC3A0;
    void *volatile surf = NULL, *volatile bb = NULL;
    FxSurfDesc8 ds, db;
    int ok = 0;
    static int s_logged;
    const char *why = NULL;
    *texOut = NULL;
    if (!tex) {
        why = "no frame-copy texture";
    } else {
        __try {
            if (FAILED(((FxGetLevel_t)VT(tex, 15))(tex, 0, (void **)&surf)) || !surf) why = "GetSurfaceLevel failed";
            else if (FAILED(((GetBB_t)VT(dev, VT_GETBACKBUFFER))(dev, 0, 0, (void **)&bb)) || !bb) why = "GetBackBuffer failed";
            else if (FAILED(((FxGetDesc_t)VT(surf, 8))(surf, &ds)) || FAILED(((FxGetDesc_t)VT(bb, 8))(bb, &db))) why = "GetDesc failed";
            else if (db.ms != 0) why = "the back buffer is multisampled";
            else if (db.fmt != ds.fmt || db.w > ds.w || db.h > ds.h || ds.w < 16 || ds.h < 16) why = "format / size mismatch";
            else if (FAILED(((FxCopyRects_t)VT(dev, 28))(dev, bb, NULL, 0, surf, NULL))) why = "CopyRects failed";
            else ok = 1, g_rnTw = (float)ds.w, g_rnTh = (float)ds.h;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ok = 0;
            why = "an exception";
        }
        __try {
            if (bb) ((Release_t)VT(bb, VT_RELEASE))(bb);
            if (surf) ((Release_t)VT(surf, VT_RELEASE))(surf);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    if (ok) {
        *texOut = tex;
        if (s_logged != 1) {
            s_logged = 1;
            Log("screen effects: Rain: refracting drops (frame copy %.0fx%.0f)", g_rnTw, g_rnTh);
        }
    } else if (s_logged != 2) {
        s_logged = 2;
        Log("screen effects: Rain: no frame copy (%s): drops without refraction (logged once per change)", why);
    }
    return ok;
}
static void FxRainDraw(void *dev, float s0, float w, float h, DWORD tick, int preview)
{
    const int p = preview ? 1 : 0;
    const LONGLONG clk = FxClock();
    const float kd = FxSat(s0 * FXN_RAIN_GAIN), aw = h > 1.0f ? 720.0f * w / h : 1280.0f;
    const int cont = g_fxFrame == g_rnFrame[p] + 1;  // SVR-106 / 201: real time between consecutive frames of this state
    RnState *s = &g_rn[p];
    void *frame = NULL, *drop = FxTexture(dev, FXX_DROP), *trail = FxTexture(dev, FXX_TRAIL), *streak = FxTexture(dev, FXX_STREAK);
    const LONGLONG dticks = cont ? clk - g_rnLast[p] : 0;
    int b, i, steps;
    static int s_stallLogged;
    g_rnLast[p] = clk;
    g_rnFrame[p] = g_fxFrame;
    if (!drop || !trail || !streak) return;
    if (!g_rnInit[p]) {
        RnInit(s, aw);
        g_rnInit[p] = 1;
    }
    if (dticks > g_fxQf.QuadPart / 2 && !s_stallLogged++)
        Log("screen effects: Rain: a %.2f s frame: the rain kept only 0.5 s of it (logged once)", (double)dticks / (double)g_fxQf.QuadPart);
    steps = RnAdvance(s, dticks, g_fxQf.QuadPart, aw, kd);
    (void)steps;
    g_rnUnit = h / 720.0f;
    g_rnDraws = 0;
    // 1. + 2. the refraction (no pixel shader: the fixed-function stages below)
    if (RnCopy(dev, &frame)) {
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x244);  // FVF XYZRHW | DIFFUSE | TEX2
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
        FxSampler(dev, frame, 0, 0);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 2 /*COLORARG1*/, 1 /*CURRENT*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 6 /*ALPHAARG2*/, 1 /*CURRENT*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 11 /*TEXCOORDINDEX*/, 1);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 13 /*ADDRESSU*/, 3 /*CLAMP*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 14 /*ADDRESSV*/, 3);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 16 /*MAGFILTER*/, 2 /*LINEAR*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 17 /*MINFILTER*/, 2);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 2, 1 /*COLOROP*/, 1 /*DISABLE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 2, 4 /*ALPHAOP*/, 1 /*DISABLE*/);
        ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 1, drop);
        RnWalk(dev, s, 0, kd, -1);
        RnFlush2(dev);
        ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 1, trail);
        RnWalk(dev, s, 1, kd, -1);
        RnFlush2(dev);
        ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 1, NULL);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 1, 11 /*TEXCOORDINDEX*/, 1);  // (FxBase's value; FxRestore puts the game's back)
    }
    // 3. the modulate layer: rims, glints, caustics, the trails' light lines (FxOverlayStates disables stage 1 again)
    FxOverlayStates(dev);
    if (g_fxNCol) ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_NCOL]);
    for (b = g_fxNCol ? 0 : -1; b < (g_fxNCol ? 8 : 0); ++b) {  // Noise Colour: one strength per draw (8 buckets)
        if (b >= 0) FxNColS(dev, ((float)b + 0.5f) / 8.0f);
        FxSampler(dev, drop, 0, 0);
        RnWalk(dev, s, 2, kd, b);
        RnFlush1(dev);
        FxSampler(dev, trail, 0, 0);
        RnWalk(dev, s, 3, kd, b);
        RnFlush1(dev);
    }
    // 3b. additive glints (ONE / ONE, no shader, the soft round glow texture): a drop catches light even in a dark room
    {
        void *glow = FxTexture(dev, FXX_GLOW);
        const int col = (int)g_fx[FX_NCOLOUR];
        const float *m = kFxNColMask[col >= 0 && col < 8 ? col : 1];
        if (glow) {
            for (i = 0; i < 3; ++i) g_rnGlintRgb[i] = col > 0 ? m[i] : 1.0f;
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 2 /*ONE*/);
            ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
            ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
            ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
            FxSampler(dev, glow, 0, 0);
            RnWalk(dev, s, 4, kd, -1);
            RnFlush1(dev);
            FxOverlayStates(dev);  // back to the 2x-modulate states for the streaks
            if (g_fxNCol) ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_NCOL]);
        }
    }
    // 4. the v3.18.0 falling streaks: 3, half strength (rain passing in front of the lens)
    FxSampler(dev, streak, 0, 0);
    for (i = 0; i < 3; ++i) {
        FxStreak *st = &g_rnStreak[p][i];
        const float t = (float)s->step / (float)RN_HZ, ks = 0.5f * kd * 0.35f;  // the state's rain clock (stops while paused)
        float ph;
        if (st->len <= 0.0f || t - st->t0 > 0.25f + 0.6f * (float)i) {
            const DWORD a = tick * 0x165667B1u + (DWORD)i * 0x27D4EB2Fu;
            st->x = FxRnd(a) * w;
            st->t0 = t + 0.4f * FxRnd(a + 1u);
            st->len = (0.10f + 0.10f * FxRnd(a + 2u)) * h;
        }
        ph = (t - st->t0) / 0.25f;
        if (ph < 0.0f || ph > 1.0f) continue;
        {
            const float y = -st->len + ph * (h + st->len);
            FxNColS(dev, ks);
            FxRect(dev, FxAlpha(ks), st->x - 1.0f * g_rnUnit, y, st->x + 1.0f * g_rnUnit, y + st->len, 0, 0, 1, 1);
        }
    }
}
