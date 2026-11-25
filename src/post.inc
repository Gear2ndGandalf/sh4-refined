// ================================================================ v3.43 PP343: Bloom, Colour Grading, Otherworld Lens
// Request: USER_FEEDBACK_v342 (the user, 2026-11-22: "go ahead and attempt ... bloom, color grading, lens effects"); the
// choices of USER_FEEDBACK_v341: each Off by default, with a preview. Plan: staging/v343_feedback/w5_post/PLAN.md. Pure
// logic: post_sim.h (test\post_sim_test.c); mock-ups: tools\post_mockup.py. Included at the end of screenfx.inc (its
// device helpers FxSave / FxBase / FxRestore / FxShader, FxVtx4, the ps.1.1 shaders FXT_PP*).
//
// Three rows on the Screen Effects page (Option 3 > Noise Effect, Enter), after Cut-scenes: Bloom Off / Low / High,
// Colour Grading Off / PS2 / Otherworld / Apartment / Bleach, Otherworld Lens Off / On ([ScreenEffects] Bloom, Grading,
// Lens; hidden PostEffects=0 removes the rows and every draw). They are drawn in the scene pass (FxSceneFrame from
// 0x4EFBFD: after Item Shine, before the HUD, so never on the HUD or a menu), before the Filter: bloom and grading on
// the frame, then the lens, then the chosen filter over them. Real-time cut-scenes follow
// the page's Cut-scenes row; the examine / message overlay as the filter; the pause keeps the copy taken with them
// (a change made in the pause redraws the clean pause picture with them, as a changed filter). The page's backdrop is
// the live preview (the lens at full strength there).
// Bloom: a frame copy (0x4036E0), the bright pass down to half size, down to 1/16 in 2x steps, two Kawase passes, up in
// 2x steps, a screen composite from the half size (post_sim.h PpMakePlan): render targets of ours (DEFAULT pool, made
// lazily, kept). They are released before a device reset at 0x402DF4 (was `call 4432F0`, EileenCurseTextureRelease, the
// first call of the reset 0x402CD0's release sequence; byte-checked with its neighbours, not a Playable Eileen hook, not
// in a cheat-table span: ct_conflicts --span 0x402DF0-0x402DFE none); without that hook there is no bloom.
// Grading: a ps.1.1 shader on the frame copy (per-channel curves: GOG's wrapper drew nothing for a dependent read, so no
// LUT). Only under the filters that keep the colours (None, CRT, Vivid, Vignette); bloom not under the night visions,
// Thermal or Spirit Vision; the lens not under Spirit Vision (post_sim.h kPp*With; the row's description says so).
// Lens: the game's haunting step's sums (0x10A3318 registered strength, 0x10A3320 the same weighted by nearness): a
// slight chromatic fringe and a breathing edge warp, fading in near ghosts and hauntings (Room 302's included).
// Failures turn the effect off for the session (logged); a fault turns all three off. Cost: the ledger's "post effects".
// v3.44 CS344 (USER_FEEDBACK_v343 items 5 / 6): Bloom Very High (about 1.6x High); the grade rebuilt without the `1-x`
// source modifier (post_sim.h's note: GOG's translator drew v3.43's as if no shader were set: the frame unchanged); the
// pixel check PxCheck (a fixed test draw through the real shader and constants into a tiny target of ours, read back),
// run by the grade at each new preset and by shadows.inc at each new Darkness: the log shows what reached the pixels.
// Hidden [ScreenEffects] PixelCheck=0 turns the checks off.
#include "post_sim.h"

typedef HRESULT(STDMETHODCALLTYPE *PpSetRT_t)(void *, void *, void *);
typedef HRESULT(STDMETHODCALLTYPE *PpTexSurf_t)(void *, UINT, void **);
enum { PPV_SETRT = 31, PPV_GETRT = 32, PPV_GETDS = 33 };

// ---------------------------------------------------------------- settings and state
static volatile LONG g_pp[PPE_N];        // the rows: 0 = Off
static LONG g_snapPp[PPE_N];             // as Options opened
static int g_ppOn;                       // the rows and the draws (PostEffects != 0, the Screen Effects hooks in place)
static int g_ppResetHook;                // 0x402DF4 is ours: bloom's render targets can be kept
static int g_ppFailed[PPE_N];            // off for this session
static int g_ppDead;                     // a fault: all three off for this session
static int g_ppLensAlways, g_ppLensOw;   // hidden [ScreenEffects] LensAlways / LensOtherworld
static float g_ppLensS, g_ppLensTarget, g_ppLensT = -1.0f;  // the lens' strength now, its target, the last update (s)
static int g_ppCopyKey = -1;             // PpKey of the settings when the pause copy was taken (-1: none yet)
static int g_ppState[PPE_N] = { -1, -1, -1 }, g_ppStateF[PPE_N] = { -1, -1, -1 };  // the last state logged (and its filter)
static volatile LONG g_ppLogQ, g_ppLensQ, g_ppNanQ;
typedef struct { void *tex, *surf; int w, h; } PpRt;
static PpRt g_ppRt[PPR_N];               // L1 .. L4, X (FRAME / BACK unused)
static void *g_ppDev;
static PpPlan g_ppPlan;
static int g_ppPlanOk, g_ppRtFails, g_ppDrawFails[PPE_N], g_ppDsMode;
static unsigned g_ppVram;
static const DWORD g_fnPpResetNext = 0x4432F0;
static int g_pxOn = 1;                   // v3.44 CS344: hidden [ScreenEffects] PixelCheck (also shadows.inc's)

// ---------------------------------------------------------------- texts (the MSG_PP0 block) and the rows
enum { PPM_BLOOM_L, PPM_GRADE_L, PPM_LENS_L, PPM_BLOOM_D, PPM_GRADE_D, PPM_LENS_D, PPM_NA_D, PPM_BLOOM_FD, PPM_GRADE_FD,
       PPM_LENS_FD, PPM_NORESET_D, PPM_V0, PPM_N = PPM_V0 + PPE_N * PP_VMAX };
typedef char PpTextsFit[PPM_N <= MSG_GT0 - MSG_PP0 ? 1 : -1];
typedef char PpRowsFit[PPE_N == FX_PP_ROWS ? 1 : -1];  // sh4windowmod.c's g_rowsFx has room for FX_PP_ROWS
static BYTE g_txtPp[PPM_N][72];
static DWORD g_ppIds[PPE_N][PP_VMAX];
static const BYTE *PpText(int id)
{
    return id >= MSG_PP0 && id < MSG_PP0 + PPM_N ? g_txtPp[id - MSG_PP0] : g_txtPp[PPM_NA_D];
}
static void PpTexts(void)
{
    int k, v;
    Encode(g_txtPp[PPM_BLOOM_L], 72, "Bloom");
    Encode(g_txtPp[PPM_GRADE_L], 72, "Colour Grading");
    Encode(g_txtPp[PPM_LENS_L], 72, "Otherworld Lens");
    Encode(g_txtPp[PPM_BLOOM_D], 72, "Bright lights glow: lamps, windows, flames, the hole (Very High: most).");  // v3.44 CS344
    Encode(g_txtPp[PPM_GRADE_D], 72, "PS2 greener and foggy, Otherworld cold, Apartment warm, Bleach hard.");
    Encode(g_txtPp[PPM_LENS_D], 72, "Near ghosts and hauntings: a slight warp and colour fringe.");
    Encode(g_txtPp[PPM_NA_D], 72, "Off this session (see SH4Refined.log).");
    Encode(g_txtPp[PPM_BLOOM_FD], 72, "Not with this Filter (it has its own light).");
    Encode(g_txtPp[PPM_GRADE_FD], 72, "Not with this Filter (it sets the colours itself).");
    Encode(g_txtPp[PPM_LENS_FD], 72, "Not with Spirit Vision.");
    Encode(g_txtPp[PPM_NORESET_D], 72, "Not available: the game's reset code is not stock (see the log).");
    for (k = 0; k < PPE_N; ++k)
        for (v = 0; v < kPpCount[k]; ++v) {
            Encode(g_txtPp[PPM_V0 + k * PP_VMAX + v], 72, kPpName[k][v]);
            g_ppIds[k][v] = MSG_PP0 + PPM_V0 + k * PP_VMAX + v;
        }
}
static int PpKeyNow(void) { return PpKey((int)g_pp[PPE_BLOOM], (int)g_pp[PPE_GRADE], (int)g_pp[PPE_LENS]); }
static void PpSet(int k, int v, int save)
{
    if (k < 0 || k >= PPE_N || v < 0 || v >= kPpCount[k]) return;
    InterlockedExchange(&g_pp[k], v);
    if (save && !g_iniFallback) WritePrivateProfileStringA("ScreenEffects", kPpIniKey[k], kPpName[k][v], g_iniPath);
    Log("screen effects: %s %s%s", kPpIniKey[k], kPpName[k][v],
        g_ppDead || g_ppFailed[k] ? " (off this session: see the post effects lines)" :
        k == PPE_BLOOM && v && !g_ppResetHook ? " (no bloom: the device reset hook is not in place)" : "");
}
static int PpInput(int k, int count)
{
    const int v = StepValue((int)g_pp[k], count);
    if (v >= 0) PpSet(k, v, 1);
    return 0;
}
static int __cdecl PpIn0(BYTE *cfg, int count) { (void)cfg; return PpInput(PPE_BLOOM, count); }
static int __cdecl PpIn1(BYTE *cfg, int count) { (void)cfg; return PpInput(PPE_GRADE, count); }
static int __cdecl PpIn2(BYTE *cfg, int count) { (void)cfg; return PpInput(PPE_LENS, count); }
static int PpGet(int k) { const int v = (int)g_pp[k]; return v >= 0 && v < kPpCount[k] ? v : 0; }
static int __cdecl PpGet0(void) { return PpGet(PPE_BLOOM); }
static int __cdecl PpGet1(void) { return PpGet(PPE_GRADE); }
static int __cdecl PpGet2(void) { return PpGet(PPE_LENS); }
static int PpFilterNow(void) { const int f = (int)g_fx[FX_FILTER]; return f >= 0 && f < PP_FILTERS ? f : 0; }
static int __cdecl PpDesc0(void)
{
    if (g_ppDead || g_ppFailed[PPE_BLOOM]) return MSG_PP0 + PPM_NA_D;
    if (!g_ppResetHook) return MSG_PP0 + PPM_NORESET_D;
    return MSG_PP0 + (g_pp[PPE_BLOOM] && !PpWith(kPpBloomWith, PpFilterNow()) ? PPM_BLOOM_FD : PPM_BLOOM_D);
}
static int __cdecl PpDesc1(void)
{
    if (g_ppDead || g_ppFailed[PPE_GRADE]) return MSG_PP0 + PPM_NA_D;
    return MSG_PP0 + (g_pp[PPE_GRADE] && !PpWith(kPpGradeWith, PpFilterNow()) ? PPM_GRADE_FD : PPM_GRADE_D);
}
static int __cdecl PpDesc2(void)
{
    if (g_ppDead || g_ppFailed[PPE_LENS]) return MSG_PP0 + PPM_NA_D;
    return MSG_PP0 + (g_pp[PPE_LENS] && !PpWith(kPpLensWith, PpFilterNow()) ? PPM_LENS_FD : PPM_LENS_D);
}
// sh4windowmod.c (the Screen Effects page's table): our rows at `rows` (7 DWORDs each); returns how many (0: none)
static int PpBuildRows(DWORD *rows)
{
    static const void *const kIn[PPE_N] = { PpIn0, PpIn1, PpIn2 };
    static const void *const kGet[PPE_N] = { PpGet0, PpGet1, PpGet2 };
    static const void *const kDesc[PPE_N] = { PpDesc0, PpDesc1, PpDesc2 };
    int k;
    if (!g_ppOn) return 0;
    PpTexts();
    for (k = 0; k < PPE_N; ++k)
        FillRow(rows + 7 * k, kIn[k], kGet[k], g_ppIds[k], kPpCount[k], MSG_PP0 + PPM_BLOOM_L + k, kDesc[k]);
    return PPE_N;
}
static void PpSnapshot(void)
{
    int k;
    for (k = 0; k < PPE_N; ++k) g_snapPp[k] = g_pp[k];
}
static int PpChanged(void)
{
    int k;
    if (!g_snapOn || !g_ppOn) return 0;
    for (k = 0; k < PPE_N; ++k)
        if (g_pp[k] != g_snapPp[k]) return 1;
    return 0;
}
static int PpRowChanged(int row)  // row 0..2 = our rows on the page (its rows FX_N ..)
{
    return g_snapOn && g_ppOn && row >= 0 && row < PPE_N && g_pp[row] != g_snapPp[row];
}
static void PpDiscard(void)
{
    int k;
    if (!g_snapOn || !g_ppOn) return;
    for (k = 0; k < PPE_N; ++k)
        if (g_pp[k] != g_snapPp[k]) PpSet(k, (int)g_snapPp[k], 1);
}

// ---------------------------------------------------------------- render targets (DEFAULT pool, game thread)
static void PpRtFree(PpRt *r)
{
    if (r->surf) ((Release_t)VT(r->surf, VT_RELEASE))(r->surf);
    if (r->tex) {
        const unsigned b = (unsigned)r->w * (unsigned)r->h * 4u;
        ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
        g_ppVram = g_ppVram >= b ? g_ppVram - b : 0;
    }
    memset(r, 0, sizeof *r);
}
static void PpFreeAll(const char *why)
{
    const unsigned had = g_ppVram;
    int k, n = 0;
    for (k = 0; k < PPR_N; ++k)
        if (g_ppRt[k].tex) ++n, PpRtFree(&g_ppRt[k]);
    g_ppPlanOk = 0;
    if (n) LogV("post effects: %s: %d render target(s) released (%.1f MB)", why, n, (float)had / (1024.0f * 1024.0f));
}
static void PpFreeOld(const char *why)  // targets of an older device (each holds a reference on it); guarded
{
    __try {
        PpFreeAll(why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("post", "releasing the bloom targets of an older Direct3D device faulted: forgotten instead (logged once)");
        memset(g_ppRt, 0, sizeof g_ppRt), g_ppVram = 0, g_ppPlanOk = 0;
    }
}
static void PpDeviceCheck(void *dev)
{
    if (dev == g_ppDev) return;
    if (g_ppDev) PpFreeOld("a new Direct3D device");
    g_ppDev = dev;
    g_ppDsMode = 0;
}
static int PpRtMake(void *dev, PpRt *r, int w, int h)
{
    HRESULT hr;
    if (r->tex && r->w == w && r->h == h) return 1;
    PpRtFree(r);
    hr = ((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, (UINT)w, (UINT)h, 1, 1 /*RENDERTARGET*/, 21 /*A8R8G8B8*/, 0 /*DEFAULT*/, &r->tex);
    if (FAILED(hr) || !r->tex) {
        r->tex = NULL;
        Log("post effects: a %dx%d render target could not be made (hr %08lX)", w, h, (unsigned long)hr);
        return 0;
    }
    hr = ((PpTexSurf_t)VT(r->tex, 15))(r->tex, 0, &r->surf);
    if (FAILED(hr) || !r->surf) {
        r->surf = NULL;
        ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
        r->tex = NULL;
        Log("post effects: a render target's surface could not be read (hr %08lX)", (unsigned long)hr);
        return 0;
    }
    r->w = w, r->h = h;
    g_ppVram += (unsigned)w * (unsigned)h * 4u;
    return 1;
}
// 0x402DF4 (was `call 4432F0`): the device reset 0x402CD0 is about to release its DEFAULT objects and Reset
static void __cdecl PpBeforeReset(void)
{
    __try {
        if (g_ppDev == DEV) PpFreeAll("device reset");
        else PpFreeOld("device reset (targets of an older device)");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("post", "releasing the bloom targets before a device reset faulted (logged once)");
    }
}
static __declspec(naked) void hkPpBeforeReset(void)
{
    __asm {
        pushad
        pushfd
        call PpBeforeReset
        popfd
        popad
        jmp dword ptr [g_fnPpResetNext]  // the stock callee returns to 0x402DF9
    }
}
static void PpFail(int e, const char *why, HRESULT hr)
{
    static const char *const kWhat[PPE_N] = { "bloom", "colour grading", "the Otherworld lens" };
    if (e < 0 || e >= PPE_N || g_ppFailed[e]) return;
    g_ppFailed[e] = 1;
    Log("post effects: %s off for this session: %s (hr %08lX)", kWhat[e], why, (unsigned long)hr);
    if (e == PPE_BLOOM) PpFreeAll("bloom off");
}
static int PpTarget(void *dev, const PpRt *r, void *gameDs)  // one of ours (no depth buffer; the game's if refused)
{
    Viewport8 vp;
    HRESULT hr = E_FAIL;
    if (g_ppDsMode != 2) {
        hr = ((PpSetRT_t)VT(dev, PPV_SETRT))(dev, r->surf, NULL);
        if (SUCCEEDED(hr) && !g_ppDsMode) g_ppDsMode = 1;
    }
    if (FAILED(hr) && g_ppDsMode != 1 && gameDs) {
        hr = ((PpSetRT_t)VT(dev, PPV_SETRT))(dev, r->surf, gameDs);
        if (SUCCEEDED(hr) && !g_ppDsMode) {
            g_ppDsMode = 2;
            LogV("post effects: no depth buffer refused: the bloom targets use the game's (Z off)");
        }
    }
    if (FAILED(hr)) return 0;
    vp.x = vp.y = 0, vp.w = (DWORD)r->w, vp.h = (DWORD)r->h, vp.minZ = 0.0f, vp.maxZ = 1.0f;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
    return 1;
}
static void PpPsc(void *dev, int reg, float a, float b, float c, float d)
{
    float v[4];
    v[0] = a, v[1] = b, v[2] = c, v[3] = d;
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, (DWORD)reg, v, 1);
}
static void PpTex4(void *dev, void *tex)
{
    int s;
    for (s = 0; s < 4; ++s) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)s, tex);
}
// a dw x dh quad (pixel centres), four tap sets at (-+du, -+dv) around the base coordinate
static HRESULT PpQuad4(void *dev, int dw, int dh, float du, float dv)
{
    static const float kS[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
    FxVtx4 q[4];
    int m, k;
    for (m = 0; m < 4; ++m) {
        q[m].x = (m & 1) ? (float)dw - 0.5f : -0.5f;
        q[m].y = (m & 2) ? (float)dh - 0.5f : -0.5f;
        q[m].z = 0.0f, q[m].rhw = 1.0f, q[m].c = 0xFFFFFFFFu;
        for (k = 0; k < 4; ++k) {
            q[m].t[k][0] = ((m & 1) ? 1.0f : 0.0f) + kS[k][0] * du;
            q[m].t[k][1] = ((m & 2) ? 1.0f : 0.0f) + kS[k][1] * dv;
        }
    }
    return ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(FxVtx4));
}
static void *PpFrame(void)
{
    float fw = 0.0f, fh = 0.0f;
    return ((void *(__cdecl *)(float *, float *))0x4036E0)(&fw, &fh);  // the frame so far (the game's screen copy)
}
static const PpPlan *PpPlanFor(int w, int h)
{
    if (g_ppPlanOk && g_ppPlan.w[PPR_FRAME] == w && g_ppPlan.h[PPR_FRAME] == h) return &g_ppPlan;
    if (!PpMakePlan(w, h, &g_ppPlan)) return NULL;
    g_ppPlanOk = 1;
    LogV("post effects: bloom chain at %dx%d: %dx%d down to %dx%d (%d levels), 2 blur passes, up in 2x steps: %d draws, "
         "%.1f MB of render targets", w, h, g_ppPlan.w[PPR_L1], g_ppPlan.h[PPR_L1], g_ppPlan.w[PPR_L4], g_ppPlan.h[PPR_L4],
         g_ppPlan.levels, g_ppPlan.nsteps, (float)PpPlanBytes(&g_ppPlan) / (1024.0f * 1024.0f));
    return &g_ppPlan;
}
// The chain up to the composite, into g_ppRt[PPR_L1]: 1 = done (the caller sets the back buffer again either way)
static int PpBloomChain(void *dev, void *frame, const PpBloomLook *L, const PpPlan *pl, void *gameDs)
{
    int k;
    for (k = PPR_L1; k <= PPR_X; ++k)
        if (!PpRtMake(dev, &g_ppRt[k], pl->w[k], pl->h[k])) {
            if (++g_ppRtFails >= 3) PpFail(PPE_BLOOM, "its render targets could not be made 3 times in a row", E_OUTOFMEMORY);
            return 0;
        }
    g_ppRtFails = 0;
    for (k = 0; k < pl->nsteps && pl->step[k].kind != PPS_COMP; ++k) {
        const PpStep *s = &pl->step[k];
        HRESULT hr;
        if (!PpTarget(dev, &g_ppRt[s->dst], gameDs)) {
            PpFail(PPE_BLOOM, "a bloom target could not be set", E_FAIL);
            return 0;
        }
        FxBase(dev);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 0xF);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
        PpPsc(dev, 0, 0.5f, 0.5f, 0.5f, 0.5f);
        if (s->kind == PPS_BRIGHT) {
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPBRIGHT]);
            PpPsc(dev, 1, L->th, L->th, L->th, 0.0f);
            PpPsc(dev, 2, L->gain * 0.25f, L->gain * 0.25f, L->gain * 0.25f, 0.0f);
        } else if (s->kind == PPS_UP) {  // over the level's own content: (1 - up) own + up the level below
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPUP]);
            PpPsc(dev, 1, 1.0f, 1.0f, 1.0f, 0.0f);
            PpPsc(dev, 2, 0.0f, 0.0f, 0.0f, L->up);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
            ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
        } else {
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPMEAN]);
        }
        PpTex4(dev, s->src == PPR_FRAME ? frame : g_ppRt[s->src].tex);
        hr = PpQuad4(dev, s->dw, s->dh, s->du, s->dv);
        if (FAILED(hr)) {
            if (++g_ppDrawFails[PPE_BLOOM] >= 3) PpFail(PPE_BLOOM, "its draws failed 3 times", hr);
            return 0;
        }
    }
    PpTex4(dev, NULL);
    return 1;
}
// the lens: the frame copy through a 16 x 10 mesh, the three channels from their own warped coordinates
static HRESULT PpLensDraw(void *dev, void *frame, float s, float w, float h)
{
    FxVtx4 v[2 * (PP_LENS_GX + 1)];
    const float aspect = w / h, t = FxSecs();
    float k[3];
    int i, j, c, r, n;
    HRESULT hr = S_OK;
    for (c = 0; c < 3; ++c) k[c] = PpLensK(s, t, c);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPLENS]);
    PpPsc(dev, 0, 1.0f, 0.0f, 0.0f, 0.0f);
    PpPsc(dev, 1, 0.0f, 1.0f, 0.0f, 0.0f);
    PpPsc(dev, 2, 0.0f, 0.0f, 1.0f, 0.0f);
    for (c = 0; c < 3; ++c) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)c, frame);
    for (j = 0; j < PP_LENS_GY && SUCCEEDED(hr); ++j) {
        n = 0;
        for (i = 0; i <= PP_LENS_GX; ++i)
            for (r = 0; r < 2; ++r) {
                const float u = (float)i / (float)PP_LENS_GX, vv = (float)(j + r) / (float)PP_LENS_GY;
                FxVtx4 *p = &v[n++];
                p->x = u * w - 0.5f, p->y = vv * h - 0.5f, p->z = 0.0f, p->rhw = 1.0f, p->c = 0xFFFFFFFFu;
                for (c = 0; c < 3; ++c) PpLensUv(u, vv, aspect, k[c], &p->t[c][0], &p->t[c][1]);
                p->t[3][0] = p->t[1][0], p->t[3][1] = p->t[1][1];
            }
        hr = ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, (UINT)(n - 2), v, sizeof(FxVtx4));
    }
    for (c = 0; c < 3; ++c) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)c, NULL);
    return hr;
}
// ---------------------------------------------------------------- v3.45 RB345: the GPU -> CPU read back (post_sim.h RbOrder)
// rt: a w x h A8R8G8B8 surface of a RENDERTARGET texture of ours (the pixel check's target, the depth probe's map).
// 1 = r->lr holds the locked copy (read it, then RbEnd); 0 = nothing read: no read back on this backend, or every path
// failed this time (the callers stay quiet: this logs the path that worked, once, or the session's "no read back", once).
// The tries run inside Interp_OwnBegin / End (as FxGrabClean's routes): r->owned tells a fault handler to balance it.
// RbEnd is safe twice and from a fault handler. Shared by shadows.inc (ShPxCheck) and dof.inc (DofProbeRun).
typedef struct { void *mem; FxLockRect8 lr; int locked, owned, path; } RbRead;
static RbState g_rb = { { 0 }, { 0 }, { 0 }, { 0 }, -1, 0 };
static volatile LONG g_rbQ;
static const char kRbQuiet[] = "no read back on this backend";  // PxCheck's *why when nothing could be read: log nothing
static int RbOff(void) { return g_rb.off; }
static void RbEnd(RbRead *r)
{
    if (r->owned) Interp_OwnEnd();
    r->owned = 0;
    if (r->locked && r->mem) ((FxSurfUnlock_t)VT(r->mem, 10))(r->mem);
    r->locked = 0;
    if (r->mem) ((Release_t)VT(r->mem, VT_RELEASE))(r->mem);
    r->mem = NULL;
}
static void RbLog(int ev, int p, UINT w, UINT h, const char *who)
{
    char was[256];
    int q, n = 0;
    was[0] = 0;
    for (q = 0; q < RBP_N && n < (int)sizeof was - 96; ++q)
        if (q != p && g_rb.fail[q])
            n += sprintf_s(was + n, sizeof was - (size_t)n, "%s%s: %s (hr %08lX, %u time(s))", n ? "; " : "", kRbPathName[q],
                           kRbStepName[g_rb.step[q]], g_rb.hr[q], g_rb.fail[q]);
    switch (ev) {
    case RBE_FIRST:
        Log("read back: GPU -> CPU through %s on this backend (CopyRects, then LockRect; first used by %s, %ux%u)%s%s%s: the pixel "
            "checks and the depth probe read back this way", kRbPathName[p], who, w, h, n ? " (before it, " : "", was, n ? ")" : "");
        break;
    case RBE_SWITCH:
        if (LogQuota(&g_rbQ, 6)) LogV("read back: now through %s (%s, %ux%u; %s)", kRbPathName[p], who, w, h, n ? was : "no other path failed");
        break;
    case RBE_LOST:
        if (LogQuota(&g_rbQ, 6))
            LogV("read back: %s failed this time (%s, %ux%u): %s (hr %08lX); it worked %u time(s) before: tried again next time",
                 kRbPathName[p], who, w, h, kRbStepName[g_rb.step[p]], g_rb.hr[p], g_rb.ok[p]);
        break;
    case RBE_OFF:
        n = 0;
        for (q = 0; q < RBP_N && n < (int)sizeof was - 96; ++q)
            n += sprintf_s(was + n, sizeof was - (size_t)n, "%s%s: %s (hr %08lX, %u time(s))", n ? "; " : "", kRbPathName[q],
                           kRbStepName[g_rb.step[q]], g_rb.hr[q], g_rb.fail[q]);
        Log("read back: no GPU -> CPU read back on this backend (%s; last by %s, %ux%u): the pixel checks are off and the depth probe "
            "does not run for this session (the effects draw as before; depth of field unprobed)", was, who, w, h);
        break;
    }
}
static int RbBegin(void *dev, void *rt, UINT w, UINT h, RbRead *r, const char *who)
{
    int order[RBP_N], n, i;
    memset(r, 0, sizeof *r);
    r->path = -1;
    n = RbOrder(&g_rb, order);
    for (i = 0; i < n; ++i) {
        const int p = order[i];
        int step = RBS_MAKE;
        HRESULT hr;
        void *mem = NULL;
        Interp_OwnBegin();
        r->owned = 1;
        hr = p == RBP_LOCKRT ? ((FxRtSurf_t)VT(dev, 25))(dev, w, h, 21 /*A8R8G8B8*/, 0 /*no MSAA*/, TRUE /*lockable*/, &mem)
                             : ((FxImgSurf_t)VT(dev, 27))(dev, w, h, 21, &mem);
        if (FAILED(hr)) mem = NULL;
        else if (!mem) hr = E_FAIL;
        r->mem = mem;
        if (SUCCEEDED(hr)) {
            step = RBS_COPY;
            hr = ((FxCopyRects_t)VT(dev, 28))(dev, rt, NULL, 0, r->mem, NULL);
        }
        if (SUCCEEDED(hr)) {
            step = RBS_LOCK;
            hr = ((FxSurfLock_t)VT(r->mem, 9))(r->mem, &r->lr, NULL, 0x10 /*READONLY*/);
            if (SUCCEEDED(hr) && !r->lr.bits) {
                ((FxSurfUnlock_t)VT(r->mem, 10))(r->mem);
                hr = E_FAIL;
            }
        }
        if (SUCCEEDED(hr)) step = RBS_OK, r->locked = 1;
        Interp_OwnEnd();
        r->owned = 0;
        RbLog(RbNote(&g_rb, p, step, (unsigned long)hr), p, w, h, who);
        if (step == RBS_OK) {
            r->path = p;
            return 1;
        }
        if (r->mem) ((Release_t)VT(r->mem, VT_RELEASE))(r->mem);
        r->mem = NULL;
    }
    return 0;
}
// ---------------------------------------------------------------- v3.44 CS344: the pixel check (also shadows.inc's)
// A fixed test draw through the real shader and constants into a tiny target of ours, read back, so the log shows what
// reached the pixels, not what the device's constant cache holds (v3.43: c1 read back right, the picture unchanged).
// The source (PX_W x PX_H) holds PX_CELLS cells of PX_CW x PX_H texels (src[k]; v3.47 PX347: was 2 wide); the destination is cleared to dst; draw()
// draws the destination's whole area (a PX_W x PX_H quad, texture coordinates 0..1 over it) from the source texture;
// out[k] = the destination's pixel in cell k. Nothing is kept (both targets are made and released here: nothing for a
// device reset to trip on); the caller's render target, depth buffer and viewport are put back, its other states are
// its own to set again (its real draw sets them). One GPU sync (the read back): callers run it at a new value only.
// v3.47 PX347: the pattern is post_sim.h's (PX_CELLS cells of PX_CW x PX_H texels, was 2 x 2 read at the first
// column): the destination is read at each cell's centre, first / last column and top row (PxProbe), the source at
// each centre (a second read back, so two GPU syncs).
typedef int (*PxDraw_t)(void *dev, void *srcTex, const void *ctx);
typedef char PxCellsFit[PX_CELLS == PP_PX_N ? 1 : -1];  // the grade's cells
typedef struct { void *tex[2], *surf[2]; RbRead rb; } PxRes;  // v3.45 RB345: the read back's (was mem / locked / owned)
static int PxSetTarget(void *dev, void *surf, void *ds)
{
    Viewport8 vp;
    if (FAILED(((PpSetRT_t)VT(dev, PPV_SETRT))(dev, surf, NULL)) && (!ds || FAILED(((PpSetRT_t)VT(dev, PPV_SETRT))(dev, surf, ds))))
        return 0;
    vp.x = vp.y = 0, vp.w = PX_W, vp.h = PX_H, vp.minZ = 0.0f, vp.maxZ = 1.0f;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
    return 1;
}
// v3.47 PX347: a texel of the locked copy
static DWORD PxTexel(const FxLockRect8 *lr, int x, int y)
{
    return *(const DWORD *)(lr->bits + (size_t)y * (size_t)lr->pitch + (size_t)x * 4u);
}
static int PxBody(void *dev, PxRes *r, void *ds, const DWORD *src, DWORD dst, PxDraw_t draw, const void *ctx, PxProbe *pr, const char **why)
{
    int k, at;
    for (k = 0; k < 2; ++k) {
        if (FAILED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, PX_W, PX_H, 1, 1 /*RENDERTARGET*/, 21 /*A8R8G8B8*/, 0 /*DEFAULT*/, &r->tex[k])) ||
            !r->tex[k]) {
            r->tex[k] = NULL;
            *why = "a test target could not be made";
            return 0;
        }
        if (FAILED(((PpTexSurf_t)VT(r->tex[k], 15))(r->tex[k], 0, &r->surf[k])) || !r->surf[k]) {
            r->surf[k] = NULL;
            *why = "a test target's surface could not be read";
            return 0;
        }
    }
    if (!PxSetTarget(dev, r->surf[0], ds)) {
        *why = "a test target could not be set";
        return 0;
    }
    for (k = 0; k < PX_CELLS; ++k) {
        Rect8 rc;
        rc.x1 = PX_CW * k, rc.y1 = 0, rc.x2 = PX_CW * (k + 1), rc.y2 = PX_H;  // v3.47 PX347: PX_CW wide (was 2)
        ((Clear_t)VT(dev, VT_CLEAR))(dev, 1, &rc, 1 /*TARGET*/, src[k], 1.0f, 0);
    }
    if (!PxSetTarget(dev, r->surf[1], ds)) {
        *why = "a test target could not be set";
        return 0;
    }
    ((Clear_t)VT(dev, VT_CLEAR))(dev, 0, NULL, 1 /*TARGET*/, dst, 1.0f, 0);
    k = draw(dev, r->tex[0], ctx);
    PpTex4(dev, NULL);
    if (!k) {
        *why = "the test draw failed";
        return 0;
    }
    if (!RbBegin(dev, r->surf[1], PX_W, PX_H, &r->rb, "a pixel check")) {  // v3.45 RB345: the path chain (was one CopyRects)
        *why = kRbQuiet;
        return 0;
    }
    for (k = 0; k < PX_CELLS; ++k)  // v3.47 PX347: the centre, first / last column and top row of each cell (was column 2k)
        for (at = 0; at < PXC_N; ++at) pr->dst[k][at] = PxTexel(&r->rb.lr, PxCol(k, at), PxRow(at));
    RbEnd(&r->rb);
    // v3.47 PX347: the source as the draw found it (its cells' centres): the sim also runs on what it held
    if (RbBegin(dev, r->surf[0], PX_W, PX_H, &r->rb, "a pixel check's source")) {
        for (k = 0; k < PX_CELLS; ++k) pr->src[k] = PxTexel(&r->rb.lr, PxCol(k, PXC_MID), PX_ROW);
        pr->srcOk = 1;
        RbEnd(&r->rb);
    }
    return 1;
}
// 1 = out[] read; else *why says what failed (the caller logs it; v3.45 RB345: not when *why == kRbQuiet: nothing could be
// read back, RbBegin logged what the session needs)
// v3.47 PX347: *pr = what was read (was out[]: one pixel a cell)
static int PxCheck(void *dev, const DWORD src[PX_CELLS], DWORD dst, PxDraw_t draw, const void *ctx, PxProbe *pr, const char **why)
{
    PxRes r;
    void *rt = NULL, *ds = NULL;
    Viewport8 vp;
    int ok = 0, k;
    memset(&r, 0, sizeof r);
    memset(pr, 0, sizeof *pr);
    *why = "";
    if (RbOff()) {  // v3.45 RB345: no read back on this backend: no targets, no draw
        *why = kRbQuiet;
        return 0;
    }
    if (FAILED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &vp)) || FAILED(((GetSurf_t)VT(dev, PPV_GETRT))(dev, &rt)) || !rt) {
        if (rt) ((Release_t)VT(rt, VT_RELEASE))(rt);
        *why = "the render target or viewport could not be read";
        return 0;
    }
    if (FAILED(((GetSurf_t)VT(dev, PPV_GETDS))(dev, &ds))) ds = NULL;
    __try {
        ok = PxBody(dev, &r, ds, src, dst, draw, ctx, pr, why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (r.rb.owned) Interp_OwnEnd();  // v3.45 RB345: the read back's
        r.rb.owned = 0;
        ok = 0;
        *why = "a fault";
    }
    __try {
        if (r.rb.locked && r.rb.mem) ((FxSurfUnlock_t)VT(r.rb.mem, 10))(r.rb.mem);  // v3.45 RB345
        r.rb.locked = 0;
        ((PpSetRT_t)VT(dev, PPV_SETRT))(dev, rt, ds);
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
        RbEnd(&r.rb);  // v3.45 RB345: releases the read back's surface
        for (k = 0; k < 2; ++k) {
            if (r.surf[k]) ((Release_t)VT(r.surf[k], VT_RELEASE))(r.surf[k]);
            if (r.tex[k]) ((Release_t)VT(r.tex[k], VT_RELEASE))(r.tex[k]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
        *why = "a fault while putting the render target back";
    }
    if (ds) ((Release_t)VT(ds, VT_RELEASE))(ds);
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    return ok;
}
// v3.47 PX347: the measured sampling offset (post_sim.h PxEdgeOffset, on the byte at shift), the raw edges, the top row:
// what the backend does to every full-screen pass of ours (logged a few times a session)
static volatile LONG g_pxAlignQ;
static void PxAlignLog(const char *who, const PxProbe *pr, int shift)
{
    char e[200];
    int k, n = 0, used = 0;
    float o;
    if (!LogQuota(&g_pxAlignQ, 6)) return;
    o = PxEdgeOffset(pr, shift, &used);
    e[0] = 0;
    for (k = 0; k < PX_CELLS && n < (int)sizeof e - 40; ++k)
        n += sprintf_s(e + n, sizeof e - (size_t)n, "%s%u / %u / %u (top %u)", k ? "; " : "", PxByte(pr->dst[k][PXC_FIRST], shift),
                       PxByte(pr->dst[k][PXC_MID], shift), PxByte(pr->dst[k][PXC_LAST], shift), PxByte(pr->dst[k][PXC_TOP], shift));
    if (used)
        LogV("pixel check alignment (%s): the test draw samples %.2f texel(s) %s of the texel centres (from %d cell edge(s); "
             "each cell's first / centre / last column, %s: %s)%s", who, fabsf(o), o < 0.0f ? "left" : "right", used,
             shift == 8 ? "green" : "the byte read", e,
             fabsf(o) >= 0.25f ? ": the backend's half-pixel rule (KB 194); the cells are read at their centres, which it does not reach" : "");
    else
        LogV("pixel check alignment (%s): not measurable (no two neighbouring cells %d levels apart; %s)", who, PX_EDGE_MIN, e);
}
// v3.47 PX347: the source cells' text ("as asked", or what each held) for the logs
static void PxSrcText(char *t, size_t cap, const PxProbe *pr, const DWORD *asked, DWORD mask, int *bad)
{
    int k, n = 0;
    *bad = 0;
    t[0] = 0;
    if (!pr->srcOk) {
        sprintf_s(t, cap, "the source not read back: the sim on the colours asked");
        return;
    }
    for (k = 0; k < PX_CELLS; ++k)
        if (!PxSrcHeld(asked[k], pr->src[k], mask)) ++*bad;
    if (!*bad) {
        sprintf_s(t, cap, "the source read back as asked");
        return;
    }
    n = sprintf_s(t, cap, "the source held other values in %d cell(s) (a Clear that did not land):", *bad);
    for (k = 0; k < PX_CELLS && n > 0 && n < (int)cap - 32; ++k)
        n += sprintf_s(t + n, cap - (size_t)n, " %08lX (asked %08lX)", pr->src[k] & mask, asked[k] & mask);
}
// the grade's test draw: as PpDraw's grade, from the source cells (c = its 6 constants)
static int PpGradeTestDraw(void *dev, void *src, const void *ctx)
{
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, (void *)ctx, 6);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPGRADE]);
    PpTex4(dev, NULL);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, src);
    return SUCCEEDED(PpQuad4(dev, PX_W, PX_H, 0.0f, 0.0f));
}
// at each new preset (on the page and in play): the four test colours through the grade, read back, against the sim
static int g_ppPxLast = -1;
static volatile LONG g_ppPxQ;
static void PpGradeCheck(void *dev, int g, float c[6][4])
{
    DWORD src[PX_CELLS];
    PxProbe pr;  // v3.47 PX347 (was out[])
    unsigned char o[PP_PX_N][3], ex[PP_PX_N][3], in[PP_PX_N][3], exR[PP_PX_N][3];
    const char *why = "";
    char cells[400], st[200], who[48];
    int k, n = 0, off = 0, v, offR = 0, vR, bad = 0, basis = 0;
    float chg = 0.0f, exChg = 0.0f, chgR = 0.0f, exChgR = 0.0f;
    if (!g_pxOn || RbOff() || g == g_ppPxLast || g <= PPG_OFF || g >= PPG_N) return;  // v3.45 RB345: + no read back
    g_ppPxLast = g;
    if (!LogQuota(&g_ppPxQ, 40)) return;
    for (k = 0; k < PP_PX_N; ++k)
        src[k] = 0xFF000000u | (DWORD)kPpPxIn[k][0] << 16 | (DWORD)kPpPxIn[k][1] << 8 | (DWORD)kPpPxIn[k][2];
    if (!PxCheck(dev, src, PP_PX_DST, PpGradeTestDraw, c, &pr, &why)) {
        if (why != kRbQuiet) LogV("post effects: colour grading %s: pixel check not run (%s)", kPpName[PPE_GRADE][g], why);  // v3.45 RB345
        return;
    }
    for (k = 0; k < PP_PX_N; ++k) {  // v3.47 PX347: the cell's centre; the source as read back (else as asked)
        const DWORD m = pr.dst[k][PXC_MID], s = pr.srcOk ? pr.src[k] : src[k];
        o[k][0] = (unsigned char)(m >> 16), o[k][1] = (unsigned char)(m >> 8), o[k][2] = (unsigned char)m;
        in[k][0] = (unsigned char)(s >> 16), in[k][1] = (unsigned char)(s >> 8), in[k][2] = (unsigned char)s;
    }
    PpPxExpect(&kPpGrade[g], ex);
    v = PpPxVerdict(o, ex, &off, &chg, &exChg);
    PxSrcText(st, sizeof st, &pr, src, 0x00FFFFFFu, &bad);
    if (v != PPX_OK && bad) {  // v3.47 PX347: the source held other colours: the shader judged on what it held
        PpPxExpectFrom(&kPpGrade[g], in, exR);
        vR = PpPxVerdictIn(o, exR, in, &offR, &chgR, &exChgR);
        if (vR == PPX_OK) v = vR, off = offR, chg = chgR, exChg = exChgR, memcpy(ex, exR, sizeof ex), basis = 1;
    }
    cells[0] = 0;
    for (k = 0; k < PP_PX_N && n < (int)sizeof cells - 72; ++k)
        n += sprintf_s(cells + n, sizeof cells - n, "%s%u,%u,%u -> %u,%u,%u -> %u,%u,%u (sim %u,%u,%u)", k ? "; " : "", kPpPxIn[k][0],
                       kPpPxIn[k][1], kPpPxIn[k][2], in[k][0], in[k][1], in[k][2], o[k][0], o[k][1], o[k][2], ex[k][0], ex[k][1], ex[k][2]);
    LogV("post effects: colour grading %s: pixel check %s: a fixed test draw through the grading shader, read back: %d level(s) "
         "off its sim at most, %.1f levels changed on average (the sim %.1f): %s", kPpName[PPE_GRADE][g], v == PPX_OK ? "ok" : "FAILED",
         off, chg, exChg, kPpPxWhy[v]);
    LogV("post effects: colour grading %s: the test colours (asked -> the source -> the cell's centre read back (sim%s)) %s; %s",
         kPpName[PPE_GRADE][g], basis ? " on the source as read back" : "", cells, st);  // v3.47 PX347: + the source, 4 cells
    sprintf_s(who, sizeof who, "colour grading %s", kPpName[PPE_GRADE][g]);
    PxAlignLog(who, &pr, 8);  // v3.47 PX347
    if (v != PPX_OK) LogAnomaly("post", "colour grading %s: the pixel check read back %s (%d levels off its sim)", kPpName[PPE_GRADE][g], kPpPxWhy[v], off);
}
// v3.43 REV343 PP343-001: what a pass changed, put back on EVERY exit of PpDraw: its normal tail and the fault handler
// of PpPass (a fault after the switch to a bloom target must not leave our target, viewport, states or references
// to the game). armed = there is something to put back (post_sim.h PpOnceTake: once a pass).
typedef struct { int armed, offRt; void *dev, *rt, *ds; FxSaved sv; Viewport8 full; } PpCtx;
static PpCtx g_ppCtx;
static void PpCleanup(void)
{
    PpCtx *c = &g_ppCtx;
    if (!PpOnceTake(&c->armed)) return;
    __try {
        if (c->offRt && c->rt) {  // still on a bloom target: the back buffer and the full viewport again
            ((PpSetRT_t)VT(c->dev, PPV_SETRT))(c->dev, c->rt, c->ds);
            ((Viewport_t)VT(c->dev, FXV_SETVP))(c->dev, &c->full);
        }
        if (c->ds) ((Release_t)VT(c->ds, VT_RELEASE))(c->ds);
        if (c->rt) ((Release_t)VT(c->rt, VT_RELEASE))(c->rt);
        c->ds = c->rt = NULL;
        c->offRt = 0;
        FxRestore(c->dev, &c->sv);  // every state, texture, shader and constant as captured (and their references)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        c->ds = c->rt = NULL;
        c->offRt = 0;
        if (!s_logged++) LogAnomaly("post", "putting the device state back after a post effects pass faulted (logged once)");
    }
}
// One pass: the effects asked (each already checked), every state put back (PpCleanup). 1 = something was drawn.
static int PpDraw(void *dev, int bloom, int grade, float lensS)
{
    PpCtx *cx = &g_ppCtx;
    void *frame = NULL;
    const float w = FxW(), h = FxH();
    const PpPlan *pl = NULL;
    int drew = 0, chain = 0;
    HRESULT hr;
    PpCleanup();  // (nothing armed: a no-op; a leftover would be put back first)
    cx->dev = dev, cx->rt = cx->ds = NULL, cx->offRt = 0;
    if (!FxSave(dev, &cx->sv, FX_NPSC)) return 0;
    cx->armed = 1;
    cx->full = cx->sv.vp;
    cx->full.x = cx->full.y = 0, cx->full.w = (DWORD)w, cx->full.h = (DWORD)h;
    if (bloom && ((pl = PpPlanFor((int)w, (int)h)) == NULL ||
                  FAILED(((GetSurf_t)VT(dev, PPV_GETRT))(dev, &cx->rt)) || !cx->rt)) {
        if (cx->rt) ((Release_t)VT(cx->rt, VT_RELEASE))(cx->rt);
        cx->rt = NULL;
        bloom = 0;
    }
    if (cx->rt && FAILED(((GetSurf_t)VT(dev, PPV_GETDS))(dev, &cx->ds))) cx->ds = NULL;
    if (bloom || grade) frame = PpFrame();
    if (frame && bloom) {
        cx->offRt = 1;  // from here a fault leaves a bloom target set: PpCleanup sets the back buffer again
        chain = PpBloomChain(dev, frame, &kPpBloom[g_pp[PPE_BLOOM]], pl, cx->ds);
        hr = ((PpSetRT_t)VT(dev, PPV_SETRT))(dev, cx->rt, cx->ds);  // the back buffer again (and the full viewport)
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &cx->full);
        if (SUCCEEDED(hr)) cx->offRt = 0;
        if (FAILED(hr)) {
            static volatile LONG s_q;
            if (LogQuota(&s_q, 5)) LogAnomaly("post", "the game's render target could not be set back (hr %08lX)", (unsigned long)hr);
            chain = grade = 0, lensS = 0.0f;
        }
    }
    if (frame && grade) {  // the frame through the grading shader (no blending)
        float c[6][4];
        float off[4][2];
        memset(off, 0, sizeof off);
        PpGradeConsts(&kPpGrade[g_pp[PPE_GRADE]], c);
        PpGradeCheck(dev, (int)g_pp[PPE_GRADE], c);  // v3.44 CS344: at a new preset (puts the render target back)
        FxBase(dev);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
        ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, 6);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPGRADE]);
        PpTex4(dev, NULL);
        ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, frame);
        FxQuad4(dev, off);
        drew = 1;
    }
    if (chain) {  // the glow, screen-blended from the half size
        const PpBloomLook *L = &kPpBloom[g_pp[PPE_BLOOM]];
        const PpStep *s = &pl->step[pl->nsteps - 1];
        FxBase(dev);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 2 /*ONE*/);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 4 /*INVSRCCOLOR*/);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_PPCOMP]);
        PpPsc(dev, 0, 0.5f, 0.5f, 0.5f, 0.5f);
        PpPsc(dev, 1, L->k, L->k, L->k, 0.0f);
        PpTex4(dev, g_ppRt[PPR_L1].tex);
        hr = PpQuad4(dev, (int)w, (int)h, s->du, s->dv);
        PpTex4(dev, NULL);
        if (FAILED(hr)) {
            if (++g_ppDrawFails[PPE_BLOOM] >= 3) PpFail(PPE_BLOOM, "its composite failed 3 times", hr);
        } else {
            drew = 1;
        }
    }
    if (lensS >= PP_LENS_MIN) {
        void *f2;
        FxBase(dev);
        if ((f2 = PpFrame()) != NULL) {  // the frame with the bloom and the grade
            hr = PpLensDraw(dev, f2, lensS, w, h);
            if (FAILED(hr)) {
                if (++g_ppDrawFails[PPE_LENS] >= 3) PpFail(PPE_LENS, "its draws failed 3 times", hr);
            } else {
                drew = 1;
            }
        }
    }
    PpCleanup();  // v3.43 REV343 PP343-001 (also from PpPass's fault handler)
    return drew;
}
// a state change of one effect logged (drawn / not under this filter / off), rate-limited
static void PpNote(int e, int state, int f, int where)
{
    static const char *const kWhat[PPE_N] = { "bloom", "colour grading", "the Otherworld lens" };
    static const char *const kWhere[4] = { "the Screen Effects page preview", "the scene", "", "the paused scene (a change in the pause)" };
    if (state == g_ppState[e] && (state != 2 || f == g_ppStateF[e])) return;
    g_ppState[e] = state, g_ppStateF[e] = f;
    if (!LogQuota(&g_ppLogQ, 200)) return;
    if (state == 1)
        LogV("post effects: %s %s drawn over %s (before the filter %s)", kWhat[e], kPpName[e][g_pp[e]], kWhere[where & 3],
             kFxName[FX_FILTER][f]);
    else if (state == 2)
        LogV("post effects: %s %s not drawn under the filter %s (%s)", kWhat[e], kPpName[e][g_pp[e]], kFxName[FX_FILTER][f],
             e == PPE_GRADE ? "it sets the colours itself" : e == PPE_BLOOM ? "it has its own light" : "Spirit Vision");
}
// where: 0 the page preview, 1 the scene, 3 the paused scene redrawn. 1 = something was drawn.
static int PpPass(int where)
{
    void *dev = DEV;
    const int f = PpFilterNow(), b = PpGet(PPE_BLOOM), g = PpGet(PPE_GRADE), l = PpGet(PPE_LENS);
    int doB, doG, k;
    float ls;
    if (!g_ppOn || g_ppDead || (!b && !g && !l)) return 0;
    if (!dev || !*(void *volatile *)0x13DC3A0 || !FxOnBackBuffer(dev)) return 0;
    FxDeviceCheck(dev);
    PpDeviceCheck(dev);
    doB = b && g_ppResetHook && !g_ppFailed[PPE_BLOOM] && PpWith(kPpBloomWith, f);
    doG = g && !g_ppFailed[PPE_GRADE] && PpWith(kPpGradeWith, f);
    ls = l && !g_ppFailed[PPE_LENS] && PpWith(kPpLensWith, f) ? (where == 0 ? 1.0f : g_ppLensS) : 0.0f;
    if (doB && !(FxShader(dev, FXT_PPBRIGHT) && FxShader(dev, FXT_PPMEAN) && FxShader(dev, FXT_PPUP) && FxShader(dev, FXT_PPCOMP)))
        PpFail(PPE_BLOOM, "a pixel shader could not be made", E_FAIL), doB = 0;
    if (doG && !FxShader(dev, FXT_PPGRADE)) PpFail(PPE_GRADE, "its pixel shader could not be made", E_FAIL), doG = 0;
    if (ls >= PP_LENS_MIN && !FxShader(dev, FXT_PPLENS)) PpFail(PPE_LENS, "its pixel shader could not be made", E_FAIL), ls = 0.0f;
    if (b) PpNote(PPE_BLOOM, doB ? 1 : PpWith(kPpBloomWith, f) ? 3 : 2, f, where);
    if (g) PpNote(PPE_GRADE, doG ? 1 : PpWith(kPpGradeWith, f) ? 3 : 2, f, where);
    if (l && ls >= PP_LENS_MIN) PpNote(PPE_LENS, 1, f, where);
    else if (l && !PpWith(kPpLensWith, f)) PpNote(PPE_LENS, 2, f, where);
    if (!doB && !doG && ls < PP_LENS_MIN) return 0;
    __try {
        return PpDraw(dev, doB, doG, ls);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        PpCleanup();  // v3.43 REV343 PP343-001: the render target, viewport, states and references put back first
        g_ppDead = 1;
        for (k = 0; k < PPE_N; ++k) g_ppFailed[k] = 1;
        LogAnomaly("post", "a post effects pass faulted (code %08lX): Bloom, Colour Grading and the Otherworld Lens are off for this session",
                   GetExceptionCode());
    }
    return 0;
}

// ---------------------------------------------------------------- the scene pass (FxSceneFrame) and the lens' strength
static int PpSceneGate(void)  // gameplay, real-time cut-scenes (the page's Cut-scenes row), the examine / message overlay
{
    const int phase = *(volatile int *)0xFD5C20;
    if (!FxGameplay()) return 0;
    if (phase == 0) return 1;
    if (phase == 1) return !FxCutOff();
    if (phase != 2 || *(volatile int *)0x12E3E2C == 2) return 0;  // a menu over the frozen copy: it has them already
    return *(volatile int *)0x107BF54 == 1 && *(volatile DWORD *)0x107BF5C == 0x50A5A0;
}
static void PpLensFrame(void)
{
    const float now = FxSecs();
    const float dt = g_ppLensT < 0.0f ? 0.0f : now - g_ppLensT;
    float sum = 0.0f, fall = 0.0f, target = 0.0f;
    int stage = 0;
    g_ppLensT = now;
    if (g_pp[PPE_LENS] == PPL_ON && FxGameplay()) {
        sum = *(volatile float *)0x10A3318;   // the haunting step's registered strength sum (0 = none this frame)
        fall = *(volatile float *)0x10A3320;  // ... weighted by (1 - distance / radius) at the player
        stage = *(volatile int *)0xFD5A90;
        if (!_finite(sum) || !_finite(fall)) {
            if (LogQuota(&g_ppNanQ, 3)) LogAnomaly("post", "the haunting sums are not numbers (%g, %g): the lens ignores them", sum, fall);
            sum = fall = 0.0f;
        }
        target = PpLensTarget(sum, fall, stage, g_ppLensOw, g_ppLensAlways);
    }
    if ((target > 0.0f) != (g_ppLensTarget > 0.0f) && LogQuota(&g_ppLensQ, 300))
        LogV("post effects: the Otherworld lens fades %s (stage %X room %X; haunting strength %.2f, nearness %.2f: %.0f %%)",
             target > 0.0f ? "in" : "out", stage, *(volatile int *)0xFD5A94, sum, sum > 0.0f ? fall / sum : 0.0f, target * 100.0f);
    g_ppLensTarget = target;
    g_ppLensS = PpLensStep(g_ppLensS, target, dt);
}
// FxSceneFrame, every scene frame (before the filter); capture = the frame whose copy becomes the pause picture
static void PpScene(int capture)
{
    if (!g_ppOn) return;
    PpLensFrame();
    if (capture) g_ppCopyKey = PpKeyNow();
    if (g_ppDead || (!g_pp[PPE_BLOOM] && !g_pp[PPE_GRADE] && !g_pp[PPE_LENS]) || !PpSceneGate()) return;
    BG_RUN(BGC_POST, PpPass(1));
}
// FxBackdropFrame: the settings changed during this pause (the frozen copy shows the old ones)
static int PpCopyStale(void) { return g_ppOn && g_ppCopyKey >= 0 && g_ppCopyKey != PpKeyNow(); }
static void PpPaused(void)  // FxBackdropFrame, over the clean pause picture it drew
{
    static int s_logged;
    if (!g_ppOn) return;
    BG_RUN(BGC_POST, PpPass(3));
    if (PpCopyStale() && !s_logged++) Log("post effects: changed during a pause: the paused scene is shown with the new settings");
}
static void PpPreview(void)  // FxBackdropFilter: the page's backdrop, under the page text
{
    if (g_ppOn) BG_RUN(BGC_POST, PpPass(0));
}

// ---------------------------------------------------------------- install (ScreenFxInstall, after its core hooks)
static void PpInstall(void)
{
    static const Site kReset = { 0x402DF0, { 0x89, 0x54, 0x24, 0x34, 0xE8, 0xF7, 0x04, 0x04, 0x00, 0x39, 0x1D, 0x40, 0x10, 0xFD, 0x00 }, 15 };  // mov [esp+34h],edx; call 4432F0; cmp [FD1040],ebx
    char buf[32];
    int k, v;
    for (k = 0; k < PPE_N; ++k) {
        IniStr("ScreenEffects", kPpIniKey[k], buf, sizeof buf);
        v = PpParse(k, buf);
        g_pp[k] = v >= 0 ? v : 0;
        if (v < 0 && buf[0]) Log("screen effects: [ScreenEffects] %s=%s is not a value of this version: Off", kPpIniKey[k], buf);
    }
    g_pxOn = GetPrivateProfileIntA("ScreenEffects", "PixelCheck", 1, g_iniPath) != 0;
    RfShaderLint();  // v3.44 CS344 / CS344-LINT-1: every Refined pixel shader (screen, shadows, depth of field): dof.inc
    if (!GetPrivateProfileIntA("ScreenEffects", "PostEffects", 1, g_iniPath)) {
        Log("post effects: off ([ScreenEffects] PostEffects=0): no Bloom, Colour Grading or Otherworld Lens rows");
        return;
    }
    g_ppLensAlways = GetPrivateProfileIntA("ScreenEffects", "LensAlways", 0, g_iniPath) != 0;
    g_ppLensOw = GetPrivateProfileIntA("ScreenEffects", "LensOtherworld", 0, g_iniPath) != 0;
    for (k = 1; k < PPG_N; ++k) {  // every preset's constants inside ps.1.1's -1..1 (a test checks it too)
        float c[6][4];
        if (!PpGradeConsts(&kPpGrade[k], c)) Log("post effects: the %s grade has a constant outside -1..1 (clamped by the card)", kPpName[PPE_GRADE][k]);
    }
    if (SiteOk(&kReset) && PokeBranch(0x402DF4, 0xE8, hkPpBeforeReset, 5)) g_ppResetHook = 1;
    else Log("post effects: the device reset code 00402DF0 is not stock (another mod?): no Bloom (its render targets could not be "
             "released before a reset); Colour Grading and the Otherworld Lens work");
    g_ppOn = 1;
    Log("post effects: in place (drawn in the scene pass before the filter; device reset 00402DF4 %s); Bloom %s, Colour Grading %s, "
        "Otherworld Lens %s%s%s", g_ppResetHook ? "hooked" : "NOT hooked", kPpName[0][g_pp[0]], kPpName[1][g_pp[1]],
        kPpName[2][g_pp[2]], g_ppLensAlways ? " (LensAlways=1: the lens everywhere, a test)" : "",
        g_ppLensOw ? " (LensOtherworld=1: also faintly in every Otherworld stage)" : "");
}
