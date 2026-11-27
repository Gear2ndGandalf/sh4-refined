// ================================================================ v3.56 PV356: the Effects page's preview (the game side)
// staging/v356_feedback/w6_preview/PLAN.md; the pure rules, the key shader and the title scene: preview_sim.h
// (test\preview_sim_test.c). Included after shadows.inc / dof.inc (its render-target type ShRt, the pause picture on the
// GPU ShPauseTex, the device-reset hook that also releases ours).
//
// 1. USER_FEEDBACK_v355 issue 2 ("Effects previews fail to show the heat in Thermal and the effects in Spirit Vision"):
//    the page shows the pause's clean picture; Thermal's hot bodies and Spirit Vision's ghost glow / soul fires are drawn
//    per body in the live scene, so a picture has none of them. Now, in the capture frame of every pause ([0x12E3E2C] == 1,
//    the frame whose copy becomes the pause picture):
//    - 0x52AEB5 / 0x4EE376 (ThermDrawC / ThermCynC): the bodies and the ghosts / souls are collected whatever the filter
//      (PvCollect; a live Thermal or Spirit Vision collects its own; ordinary frames cost one memory read);
//    - 0x4EFBFD (FxSceneFrame, right after FxCaptureClean kept the clean picture on the GPU): each look's own parts are
//      drawn once over a black frame (the heat redraw ThRedraw + the footprints; Spirit Vision's ghost glow, auras,
//      portals, veins and soul fires) and copied (CopyRects, no read back) to a target of ours; the clean picture is then
//      drawn back, so the frame, its pause copy and everything after are as before. One capture per pause, on the GPU.
//    - the page (0x526A53, FxBackdropFilter after FxPass(0)) and a pause frame whose filter changed in Options
//      (FxBackdropFrame after FxPass(3)) lay the layer of the filter over the filtered picture: Thermal through the luma
//      key (dst x (1 - sat(8 L)), then + the layer), Spirit Vision added. Only for this pause's own picture (its seq).
// 2. Improvement 19 ("an efficient stock preview during title menu"): with no paused scene the page draws SH4 Refined's
//    own room, a figure and a ghost (PvSceneDraw: vertex-coloured triangles, five draws, the lists built once per size),
//    instead of PreviewImage (the F7 picture, which may hold nobody). After the filter: Thermal the figure's heat and her
//    footprints (the ghost is cold: not drawn), Spirit Vision the ghost's glow through the cabinet and the figure's soul
//    fire (spiritfire_sim.h's fire, its own state, stepped only while the page is open). [ScreenEffects] PreviewScene=
//    Picture keeps PreviewImage first (as v3.55).
// Not in the previews: Death Presence, Echo and After Shock's own looks (Powers), the heat of bodies that were hidden in
// the pause frame by another effect; the title scene shows no hauntings / portals (the pause's layer does).
#include "preview_sim.h"

static int g_pvOn = 1;              // [ScreenEffects] PreviewLayers
static int g_pvSceneOn = 1;         // [ScreenEffects] PreviewScene: 1 Scene, 0 Picture
static int g_pvFails;               // capture faults / failed restores (PV_FAILS_OFF: the layers off for the session)
static volatile LONG g_pvCapDraws;  // v3.56 MERGE: captures that drew this session (PvCapNew: the budget / rate guard skip them)
// v3.56 MERGE (PV356 x PW356 / FR356): 1 when a capture drew since the caller's last look (its own counter, updated)
static int PvCapNew(LONG *seen)
{
    const LONG n = g_pvCapDraws;
    const int moved = n != *seen;
    *seen = n;
    return moved;
}
static int g_pvSceneFails;          // title-scene faults (1: the scene off for the session)
// g_pvNoZw (the preview's heat redraw writes no depth) and g_pvColl (its own GvCollect logs nothing): screenfx.inc
static void *g_pvDev;
static ShRt g_pvHeat, g_pvSpirit;   // the layers (render targets: the back buffer's size and format)
static LONG g_pvSeq = -1;           // the pause picture (g_fxPausePicSeq) the layers belong to
static int g_pvHave;                // PVL_* drawn into them
static int g_pvBodies, g_pvPrints, g_pvGhosts, g_pvFires, g_pvAuras;  // what they hold (the page's log line)
static unsigned g_pvVram;
static DWORD g_pvPsKey;             // the key shader (0 = not made); g_pvPsKeyFailed: not again on this device
static int g_pvPsKeyFailed;
static int g_pvWasPaused, g_pvKeptLogged, g_pvPageWas, g_pvNoteKey = -1;
static int g_pvBack = -1;           // PVB_* FxBackdropC drew in g_pvBackFrame
static LONG g_pvBackFrame = -1;
static volatile LONG g_pvQCap, g_pvQWhy, g_pvQFail, g_pvQPage;  // log quotas
static int g_pvWhyLast = -1;
static PvScene g_pvScene;           // the title scene (PvSceneBuild once per size / 2D scale)
static SfWorld g_pvSfW;             // its soul fire (its own state: play's fires are not touched)
static SfSpr g_pvSfSpr[SF_SPR_MAX];
static PvTV g_pvFireV[PV_FIRE_MAX];
static SpVtx g_pvVb[PV_ROOM_MAX > PV_FIRE_MAX ? PV_ROOM_MAX : PV_FIRE_MAX];
static LONG g_pvFireFrame = -10;
#define PV_WARM_STEPS 20            // Spirit Vision's auras stepped 2 s (0.1 s steps) at a capture when it is not live

// ---------------------------------------------------------------- settings (ScreenFxInstall)
static void PvInstall(void)
{
    char buf[32];
    int known;
    g_pvOn = IniInt("ScreenEffects", "PreviewLayers") != 0;
    IniStr("ScreenEffects", "PreviewScene", buf, sizeof buf);
    g_pvSceneOn = PvParseScene(buf, &known);
    if (!known) Log("screen effects: [ScreenEffects] PreviewScene=%s is not a value (Scene, Picture): Scene", buf);
    Log("screen effects: the Effects page's preview: with no paused game %s; a pause keeps Thermal's heat and Spirit Vision's "
        "ghosts and soul fires for it: %s", g_pvSceneOn ? "SH4 Refined's own scene (PreviewScene=Scene)" :
        "PreviewImage, else SH4 Refined's own scene (PreviewScene=Picture)", g_pvOn ? "on (PreviewLayers=1)" : "off (PreviewLayers=0)");
}
// the Filter row's PreviewKey sentence: the F7 picture is not shown on the Effects page while the scene is
static int PvF7Hidden(void) { return g_pvSceneOn; }

// ---------------------------------------------------------------- render targets (DEFAULT pool, released before a reset)
static void PvRtFree(ShRt *r)
{
    const unsigned b = ShPicBytes(r->w, r->h);
    if (r->surf) ((Release_t)VT(r->surf, VT_RELEASE))(r->surf);
    if (r->tex) ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
    if (r->tex) g_pvVram = g_pvVram >= b ? g_pvVram - b : 0;
    memset(r, 0, sizeof *r);
}
static int PvRtMake(void *dev, ShRt *r, int w, int h, DWORD fmt, const char *what, int *made)
{
    HRESULT hr;
    if (r->tex && r->w == w && r->h == h && r->fmt == fmt) return 1;
    PvRtFree(r);
    hr = ((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, (UINT)w, (UINT)h, 1, 1 /*RENDERTARGET*/, fmt, 0 /*DEFAULT*/, &r->tex);
    if (FAILED(hr) || !r->tex) {
        r->tex = NULL;
        Log("screen effects: the %s (%dx%d, format %lu) could not be made (hr %08lX): that look shows no layer this pause", what, w, h,
            fmt, (unsigned long)hr);
        return 0;
    }
    hr = ((ShTexSurf_t)VT(r->tex, 15))(r->tex, 0, &r->surf);
    if (FAILED(hr) || !r->surf) {
        r->surf = NULL;
        ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
        r->tex = NULL;
        Log("screen effects: the %s's surface could not be read (hr %08lX)", what, (unsigned long)hr);
        return 0;
    }
    r->w = w, r->h = h, r->fmt = fmt;
    g_pvVram += ShPicBytes(w, h);
    *made = 1;
    LogV("screen effects: render target made: the %s %dx%d (%.1f MB; the preview's now %.1f MB)", what, w, h, ShMb(ShPicBytes(w, h)), ShMb(g_pvVram));
    return 1;
}
static void PvFreeAll(const char *why)
{
    const unsigned had = g_pvVram;
    const int n = (g_pvHeat.tex != NULL) + (g_pvSpirit.tex != NULL);
    PvRtFree(&g_pvHeat);
    PvRtFree(&g_pvSpirit);
    g_pvSeq = -1, g_pvHave = 0, g_pvKeptLogged = 0;
    if (n) LogV("screen effects: the preview's layers released (%s): %d target(s), %.1f MB", why, n, ShMb(had));
}
// shadows.inc ShBeforeReset (0x402E0D, before the game's Reset): ours go too (DEFAULT pool); an older device's: guarded
static void PvBeforeReset(void)
{
    __try {
        PvFreeAll(g_pvDev == DEV ? "device reset" : "device reset (targets of an older device)");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("screen effects", "releasing the preview's layers before a device reset faulted: forgotten (logged once)");
        memset(&g_pvHeat, 0, sizeof g_pvHeat), memset(&g_pvSpirit, 0, sizeof g_pvSpirit), g_pvVram = 0, g_pvSeq = -1, g_pvHave = 0;
    }
}
static void PvDevCheck(void *dev)
{
    if (dev == g_pvDev) return;
    if (g_pvDev) PvBeforeReset();  // (an older device's targets: released, guarded)
    g_pvDev = dev;
    memset(&g_pvHeat, 0, sizeof g_pvHeat), memset(&g_pvSpirit, 0, sizeof g_pvSpirit), g_pvVram = 0;
    g_pvPsKey = 0, g_pvPsKeyFailed = 0;
}
static int PvKeyShader(void *dev)
{
    HRESULT hr;
    int at;
    const char *why;
    if (g_pvPsKey) return 1;
    if (g_pvPsKeyFailed) return 0;
    if (PsLint(kPvPsKey, 64, &at, &why)) {  // (the native test checks it; a later edit is caught here too)
        g_pvPsKeyFailed = 1;
        LogAnomaly("screen effects", "the preview's key shader uses a form not proven through GOG's translator (instruction %d: %s): "
                   "Thermal's layer is added instead", at, why);
        return 0;
    }
    hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kPvPsKey, &g_pvPsKey);
    if (FAILED(hr) || !g_pvPsKey) {
        g_pvPsKey = 0, g_pvPsKeyFailed = 1;
        Log("screen effects: CreatePixelShader (the preview's heat key) failed (hr %08lX): Thermal's layer is added instead", (unsigned long)hr);
        return 0;
    }
    Log("screen effects: the preview's heat key shader made (ps.1.1, handle %lu, device %p)", g_pvPsKey, dev);
    return 1;
}

// ---------------------------------------------------------------- 1. the capture frame
static void PvInNow(PvCapIn *in)
{
    in->on = g_pvOn;
    in->shOn = g_shOn;
    in->hooks = g_thermHooks && g_fxOn && g_spInstalled;
    in->gameplay = FxGameplay();
    in->phase = *(volatile int *)0xFD5C20;
    in->capture = *(volatile int *)0x12E3E2C;
    in->heatOk = ThHeatOk();
    in->edgesOk = GvEdgesOk();
    in->heatLive = ThermHeatLive();
    in->ghostLive = GvLive();
    in->fails = g_pvFails;
}
// ThermDrawC's body rule without its hiding, its fallback look and its footprints (they belong to the live Thermal)
static void PvHeatAdd(const BYTE *task, BYTE *model)
{
    int type = 0, cls = ThermClassify(task, 0, NULL, &type);
    float heat = 1.0f;
    if (cls == 2) return;  // ghosts are cold
    if (cls == 0 && type == 0 && ThermLiving(task, model)) cls = 1;
    if (cls != 1) return;
    if (type > 0) {
        heat = DfHeat(task, g_thermCoolS);  // a dead enemy cools
        if (heat <= 0.0f) return;
    }
    ThAdd(task, model, heat * DpHeatK(task));
}
// 0x52AEB5 (ThermDrawC, first): in the capture frame only, the lists a live look does not collect itself
static void PvCollect(const BYTE *task, BYTE *model)
{
    PvCapIn in;
    int m;
    if (!g_pvOn || !model || *(volatile int *)0x12E3E2C != 1) return;  // every other frame: this one read
    PvInNow(&in);
    m = PvCollectMask(&in);
    // a ghost the live Thermal hides (ThermDrawC skips its draw) is not posed / drawn in this frame: no glow redraw of it
    if ((m & PVL_SPIRIT) && !(ThermalLive() && ThermClassify(task, 0, NULL, NULL) == 2)) {
        g_pvColl = 1;
        GvCollect(task, model);
        g_pvColl = 0;
    }
    if (m & PVL_HEAT) PvHeatAdd(task, model);
}
// 0x4EE376 (ThermCynC, first): the trapped Cynthia (her own draw, before the registry loop)
static void PvCyn(BYTE *model)
{
    PvCapIn in;
    int m;
    if (!g_pvOn || !model || *(volatile int *)0x12E3E2C != 1) return;
    PvInNow(&in);
    m = PvCollectMask(&in);
    if (m & PVL_SPIRIT) {
        GvFrameBegin();
        SvSoulAdd(NULL, model, SV_SOUL_NPC, 1.0f, -1.0f);
    }
    if (m & PVL_HEAT) ThAdd(NULL, model, 1.0f);
}
static void PvBlack(void *dev, float w, float h)  // the whole target black (a quad: no Clear), our base states
{
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
    FxRect(dev, 0xFF000000u, 0, 0, w, h, 0, 0, 0, 0);
}
static void PvFullVp(void *dev, const FxSaved *sv, float w, float h)
{
    Viewport8 full;
    full.x = full.y = 0, full.w = (DWORD)w, full.h = (DWORD)h, full.minZ = sv->vp.minZ, full.maxZ = sv->vp.maxZ;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &full);
}
// the heat layer's footprints (ThHeatPass's print part; no new prints: only a live Thermal steps its walkers)
static int PvPrintsDraw(void *dev, FxSaved *sv)
{
    static const float kPrint[3] = { TP_HEAT_R, TP_HEAT_G, TP_HEAT_B };
    const Viewport8 *vp = g_thVpOk && g_thStamp == g_fxFrame ? &g_thVp : &sv->vp;
    int n;
    if (!SvEye(g_svEye)) return 0;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    sv->vpSet = 1;
    FxBase(dev);
    ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
    SvNearFar();
    SvUntextured(dev);
    SvZ(dev, 0);
    g_svMul = 1.0f;
    g_svPull = 0.0f;
    n = TpDraw(dev, vp, SvSecs(), kPrint);
    GvFlush(dev);
    SvZOff(dev);
    return n;
}
// the spirit layer: GvPass's gameplay parts (the ghost glow, the auras, portals, veins, the soul fires), without Echo /
// Death Presence and the test probes. When Spirit Vision is live, GvPass steps the auras later in this frame: here they
// are only laid out on this frame's camera; else 2 s of them are stepped first (the particles fill in).
static void PvSpiritDraw(void *dev, FxSaved *sv)
{
    const Viewport8 *const oldVp = g_svVp;
    const int live = GvLive();
    const float now = SvSecs();
    float eye[3];
    int mode = 0, k, eyeOk;
    FxBase(dev);
    ((void(__cdecl *)(float *))0x55BF50)(g_gvM);
    if (g_gvStamp == g_fxFrame && g_gvN) {
        mode = GvRedraw(dev, sv, &g_pvGhosts) ? 1 : 2;
        FxBase(dev);
    }
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv->vp);  // the 3D viewport (FxSave kept it)
    sv->vpSet = 1;
    SvNearFar();
    eyeOk = SvEye(eye);
    if (!eyeOk) eye[0] = eye[1] = eye[2] = 0.0f;
    memcpy(g_svEye, eye, sizeof g_svEye);
    if (live) {
        SvAuraFrame(&sv->vp, now, 0.0f, eyeOk);
    } else {
        for (k = PV_WARM_STEPS; k >= 0; --k) {
            const float t = now - 0.1f * (float)k, dt = k == PV_WARM_STEPS ? 0.0f : 0.1f;
            SvTrackStep(t, dt);
            SvAuraFrame(&sv->vp, t, dt, eyeOk);
        }
    }
    g_pvAuras = g_svSprN;
    for (k = 0; k < (GV_DEPTH ? 2 : 1); ++k) {
        SvUntextured(dev);
        SvZ(dev, GV_DEPTH ? k + 1 : 0);
        g_pvAuras += SvPortals(dev, &sv->vp, now);
        GvFlush(dev);
        SvAuraDraw(dev);
        SvVeinsDraw(dev);
        g_pvFires = SvFlames(dev, &sv->vp, now);
    }
    SvZOff(dev);
    SvUntextured(dev);
    if (mode == 2) {
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
        g_pvGhosts = GvSprites(dev, &sv->vp, FxPulse());
    }
    g_svVp = oldVp;  // (a pointer to our stack: the live passes set their own before use)
}
// the clean pause picture back over the whole frame (point-sampled 1:1); 1 = drawn
static int PvRestore(void *dev, void *pic, float w, float h)
{
    SpVtx q[4];
    int m;
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    FxSampler(dev, pic, 0, 1);
    for (m = 0; m < 4; ++m) {
        q[m].x = ((m & 1) ? w : 0.0f) - 0.5f, q[m].y = ((m & 2) ? h : 0.0f) - 0.5f, q[m].z = 0.0f, q[m].rhw = 1.0f;
        q[m].c = 0xFFFFFFFFu, q[m].u = (m & 1) ? 1.0f : 0.0f, q[m].v = (m & 2) ? 1.0f : 0.0f;
    }
    return SUCCEEDED(((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(SpVtx)));
}
static void PvWhyNote(int why)
{
    if (why == g_pvWhyLast || why == PVW_OFF || !LogQuota(&g_pvQWhy, 40)) return;
    g_pvWhyLast = why;
    LogV("screen effects: the pause's look layers not kept: %s (Thermal / Spirit Vision show their palette / edges on the "
         "Effects page from this pause)", kPvWhy[why]);
}
// 0x4EFBFD, the capture frame, right after FxCaptureClean (FxSceneFrame): the back buffer is still the clean frame
static void PvCapture(void)
{
    void *dev = DEV, *rt = NULL, *pic = NULL;
    PvCapIn in;
    FxSurfDesc8 d;
    FxSaved sv;
    UINT pw = 0, ph = 0;
    LARGE_INTEGER t0, t1, qf;
    int want, why, made = 0, fault = 0, restored = 0, saved = 0, have = 0, copyBad = 0;
    float w = 0.0f, h = 0.0f;
    if (!g_pvOn) return;
    PvInNow(&in);
    g_pvSeq = -1, g_pvHave = 0;  // older layers never stand for this pause
    g_pvBodies = g_pvPrints = g_pvGhosts = g_pvFires = g_pvAuras = 0;
    if (dev) pic = ShPauseTex(dev, g_fxPausePicSeq, &pw, &ph);
    want = PvCaptureMask(&in, pic != NULL, g_thStamp == g_fxFrame && g_thN > 0, g_gvStamp == g_fxFrame && (g_gvN > 0 || g_svSoulN > 0), &why);
    if (!want) {
        PvWhyNote(why);
        return;
    }
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&t0);
    __try {
        PvDevCheck(dev);
        if (!FxOnBackBuffer(dev) || FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) || !rt) rt = NULL;
        if (rt && (FAILED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) || (d.fmt != 21 && d.fmt != 22) || d.w != pw || d.h != ph)) {
            PvWhyNote(PVW_NOPIC);  // (the target is not the picture's size / format: nothing to restore from 1:1)
            ((Release_t)VT(rt, VT_RELEASE))(rt);
            rt = NULL;
        }
        if (rt) {
            w = (float)d.w, h = (float)d.h;
            if ((want & PVL_HEAT) && !PvRtMake(dev, &g_pvHeat, (int)d.w, (int)d.h, d.fmt, "pause's heat layer", &made)) want &= ~PVL_HEAT;
            if ((want & PVL_SPIRIT) && !PvRtMake(dev, &g_pvSpirit, (int)d.w, (int)d.h, d.fmt, "pause's spirit layer", &made)) want &= ~PVL_SPIRIT;
            saved = want && FxSave(dev, &sv, FX_NPSC);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fault = 1;
    }
    if (!saved) {
        if (rt) ((Release_t)VT(rt, VT_RELEASE))(rt);
        if (fault && ++g_pvFails && LogQuota(&g_pvQFail, 5))
            Log("screen effects: the pause's look layers faulted before drawing (%d of %d before they are off)", g_pvFails, PV_FAILS_OFF);
        return;
    }
    InterlockedIncrement(&g_pvCapDraws);  // v3.56 MERGE: this frame's interval is not a play sample (budget, rate guard)
    __try {
        sv.vpSet = 1;
        if (want & PVL_HEAT) {
            PvFullVp(dev, &sv, w, h);
            PvBlack(dev, w, h);
            g_pvPrints = PvPrintsDraw(dev, &sv);
            FxBase(dev);
            InterlockedExchange(&g_pvNoZw, 1);
            ThRedraw(dev, &sv, &g_pvBodies);
            InterlockedExchange(&g_pvNoZw, 0);
            if (g_pvBodies + g_pvPrints > 0) {
                if (SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvHeat.surf, NULL))) have |= PVL_HEAT;
                else copyBad = 1;
            }
        }
        if (want & PVL_SPIRIT) {
            PvFullVp(dev, &sv, w, h);
            PvBlack(dev, w, h);
            PvSpiritDraw(dev, &sv);
            if (g_pvGhosts + g_pvFires + g_pvAuras > 0) {
                if (SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvSpirit.surf, NULL))) have |= PVL_SPIRIT;
                else copyBad = 1;
            }
        }
    } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
        fault = 1;
        InterlockedExchange(&g_pvNoZw, 0);
        InterlockedExchange(&g_thMode, 0);
        InterlockedExchange(&g_gvGlow, 0);
    }
    __try {  // always: the clean frame back (the pause copy at the frame's end and every pass after this one see it)
        PvFullVp(dev, &sv, w, h);
        restored = PvRestore(dev, pic, w, h);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        restored = 0;
    }
    __try {
        FxRestore(dev, &sv);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    QueryPerformanceCounter(&t1);
    if (!restored) {  // the frame may now hold a layer instead of the scene: never again this session
        g_pvFails = PV_FAILS_OFF;
        have = 0;
        LogAnomaly("screen effects", "the clean pause picture could not be drawn back after the look layers: this pause's picture may "
                   "show them; the layers are off for this session");
    } else if (fault || copyBad) {
        ++g_pvFails;
        if (LogQuota(&g_pvQFail, 5))
            Log("screen effects: the pause's look layers %s (%s%08lX at %p; %d of %d before they are off); the clean picture was drawn back",
                fault ? "faulted" : "could not be copied", fault ? "code " : "", fault ? g_gvFaultCode : 0UL, fault ? g_gvFaultAt : NULL,
                g_pvFails, PV_FAILS_OFF);
    }
    g_pvHave = have;
    g_pvSeq = have ? g_fxPausePicSeq : -1;
    g_pvWhyLast = -1;
    if (have && LogQuota(&g_pvQCap, 80)) {
        const double ms = qf.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)qf.QuadPart : 0.0;
        LogV("screen effects: the pause's look layers kept for the Effects page: heat %s (%d bod%s, %d footprint(s)), spirit %s (%d ghost(s), "
             "%d soul fire(s), %d aura sprite(s)); %dx%d, %s (the preview's %.1f MB), %.2f ms", have & PVL_HEAT ? "on" : "off", g_pvBodies,
             g_pvBodies == 1 ? "y" : "ies", g_pvPrints, have & PVL_SPIRIT ? "on" : "off", g_pvGhosts, g_pvFires, g_pvAuras, (int)w, (int)h,
             made ? "targets made" : "targets kept from an earlier pause", ShMb(g_pvVram), ms);
    }
}

// ---------------------------------------------------------------- 2. the title scene
static int PvSceneReady(void)
{
    const float w = FxW(), h = FxH(), S = SH4WM_Scale2D();
    if (g_pvScene.built && g_pvScene.w == w && g_pvScene.h == h && g_pvScene.S == (S > 0.0f ? S : 1.0f)) return 1;
    if (!PvSceneBuild(&g_pvScene, w, h, S)) {
        if (!g_pvSceneFails++) Log("screen effects: the preview scene could not be laid out for %.0fx%.0f (scale %.2f): not drawn", w, h, S);
        return 0;
    }
    LogV("screen effects: the preview scene laid out for %.0fx%.0f: %d room, %d light, %d figure, %d ghost, %d cabinet vertices", w, h,
         g_pvScene.nRoom, g_pvScene.nGlow, g_pvScene.nFig, g_pvScene.nGhost, g_pvScene.nFront);
    return 1;
}
static void PvUntex(void *dev)
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
}
static void PvBlend(void *dev, DWORD src, DWORD dst)  // 0 = off
{
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, src ? 1 : 0);
    if (src) {
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, src);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, dst);
    }
}
static int PvList(void *dev, const PvV *v, int n)  // a TRIANGLELIST of the scene's vertices (pixel centres: -0.5)
{
    int i;
    if (n < 3) return 1;
    for (i = 0; i < n; ++i) {
        g_pvVb[i].x = v[i].x - 0.5f, g_pvVb[i].y = v[i].y - 0.5f, g_pvVb[i].z = 0.0f, g_pvVb[i].rhw = 1.0f;
        g_pvVb[i].c = v[i].c, g_pvVb[i].u = g_pvVb[i].v = 0.0f;
    }
    return SUCCEEDED(((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n / 3), g_pvVb, sizeof(SpVtx)));
}
// FxBackdropC (0x526A53): the scene as the page's backdrop (the ghost not under Thermal: ghosts are cold there); 1 = drawn
static int PvSceneDraw(void *dev)
{
    FxSaved sv;
    int ok = 0;
    if (g_pvSceneFails || !PvSceneReady() || !FxSave(dev, &sv, 0)) return 0;
    __try {
        FxBase(dev);
        PvUntex(dev);
        ok = PvList(dev, g_pvScene.room, g_pvScene.nRoom);
        PvBlend(dev, 2 /*ONE*/, 2 /*ONE*/);
        ok = PvList(dev, g_pvScene.glow, g_pvScene.nGlow) && ok;
        PvBlend(dev, 0, 0);
        ok = PvList(dev, g_pvScene.fig, g_pvScene.nFig) && ok;
        if (g_fx[FX_FILTER] != FXF_THERMAL) {
            PvBlend(dev, 5 /*SRCALPHA*/, 6 /*INVSRCALPHA*/);
            ok = PvList(dev, g_pvScene.ghost, g_pvScene.nGhost) && ok;
            PvBlend(dev, 0, 0);
        }
        ok = PvList(dev, g_pvScene.front, g_pvScene.nFront) && ok;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
        g_pvSceneFails = 1;
        LogAnomaly("screen effects", "the preview scene's draw faulted: off for this session (PreviewImage / the hallway instead)");
    }
    FxRestore(dev, &sv);
    if (!ok && !g_pvSceneFails) {
        g_pvSceneFails = 1;
        Log("screen effects: the preview scene could not be drawn: off for this session (PreviewImage / the hallway instead)");
    }
    return ok;
}
// the scene's soul fire: stepped once a frame while the page shows (a fresh fire after the page was closed)
static int PvFireStep(float unit, int *nPre)
{
    SfIn in;
    int n;
    if (g_fxFrame - g_pvFireFrame > 3 || g_fxFrame < g_pvFireFrame) memset(&g_pvSfW, 0, sizeof g_pvSfW);
    g_pvFireFrame = g_fxFrame;
    PvFireIn(&in);
    n = SfFrame(&g_pvSfW, &in, 1, SvSecs(), g_pvSfSpr, SF_SPR_MAX);
    return PvFireQuads(g_pvSfSpr, n, g_pvScene.chestX, g_pvScene.chestY, g_pvScene.figH / PV_PERSON_U, unit, g_pvFireV, PV_FIRE_MAX, nPre);
}
static int PvFireDraw(void *dev)
{
    void *tex = FxTexture(dev, FXX_FLAME);
    int n, pre, b, i, ok = 1;
    if (!tex) return 0;
    n = PvFireStep(FxH() / 720.0f, &pre);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    FxSampler(dev, tex, 0, 0);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 0 /*DIFFUSE*/);
    for (b = 0; b < 2; ++b) {  // the smoke premultiplied (ONE / INVSRCALPHA), then the fire additive (ONE / ONE), as SvFlames
        const int i0 = b ? pre : 0, i1 = b ? n : pre;
        if (i1 - i0 < 3) continue;
        for (i = i0; i < i1; ++i) {
            SpVtx *o = &g_pvVb[i - i0];
            o->x = g_pvFireV[i].x - 0.5f, o->y = g_pvFireV[i].y - 0.5f, o->z = 0.0f, o->rhw = 1.0f;
            o->c = g_pvFireV[i].c, o->u = g_pvFireV[i].u, o->v = g_pvFireV[i].v;
        }
        PvBlend(dev, 2 /*ONE*/, b ? 2 /*ONE*/ : 6 /*INVSRCALPHA*/);
        ok = SUCCEEDED(((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)((i1 - i0) / 3), g_pvVb, sizeof(SpVtx))) && ok;
    }
    return ok;
}

// ---------------------------------------------------------------- the page (and a pause frame whose filter changed)
// FxBackdropC: what the page shows with no pause picture (PVB_SCENE / PVB_PICTURE / PVB_STOCK); the picture file is
// loaded only when it decides
static int PvBackPick(void *dev)
{
    PvBackIn in;
    in.pausePic = 0;
    in.sceneOn = g_pvSceneOn;
    in.sceneOk = !g_pvSceneFails;
    in.picture = PvBackNeedsPicture(0, in.sceneOn, in.sceneOk) ? FxPrevLoad(dev) != NULL : 0;
    return PvBackdrop(&in);
}
static void PvBackNote(int back)
{
    g_pvBack = back;
    g_pvBackFrame = g_fxFrame;
}
// the layer of the filter over the filtered picture (pw x ph, aspect-filled as FxDrawPic): Thermal keyed, Spirit added
static int PvLayerDraw(void *dev, void *tex, UINT pw, UINT ph, int key)
{
    FxSaved sv;
    float w, h, a, b, u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    int keyed = 0;
    if (!tex || !pw || !ph || !FxSave(dev, &sv, 2)) return 0;
    FxBase(dev);
    w = FxW(), h = FxH(), a = w / h, b = (float)pw / (float)ph;
    if (b > a) u0 = 0.5f - 0.5f * a / b, u1 = 1.0f - u0;
    else v0 = 0.5f - 0.5f * b / a, v1 = 1.0f - v0;
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    FxSampler(dev, tex, 0, 0);
    if (key && PvKeyShader(dev)) {  // pass 1: the world x (1 - sat(8 L)) where the layer has a body
        float c[2][4] = { { 0.299f, 0.587f, 0.114f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } };
        ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, c, 2);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_pvPsKey);
        PvBlend(dev, 1 /*ZERO*/, 3 /*SRCCOLOR*/);
        FxRect(dev, 0xFFFFFFFFu, 0, 0, w, h, u0, v0, u1, v1);
        keyed = 1;
    }
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);  // pass 2: + the layer
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    PvBlend(dev, 2 /*ONE*/, 2 /*ONE*/);
    FxRect(dev, 0xFFFFFFFFu, 0, 0, w, h, u0, v0, u1, v1);
    FxRestore(dev, &sv);
    return keyed || !key ? 1 : 2;
}
static int PvSceneLayer(void *dev, int heat)  // the title scene's own heat / spirit, after the filter
{
    FxSaved sv;
    int ok;
    if (!g_pvScene.built || !FxSave(dev, &sv, 0)) return 0;
    FxBase(dev);
    PvUntex(dev);
    if (heat) {
        PvBlend(dev, 5 /*SRCALPHA*/, 6 /*INVSRCALPHA*/);
        ok = PvList(dev, g_pvScene.heat, g_pvScene.nHeat);
    } else {
        PvSceneSpirit(&g_pvScene, g_gvBloodOn, FxPulse());
        PvBlend(dev, 2 /*ONE*/, 2 /*ONE*/);
        ok = PvList(dev, g_pvScene.spirit, g_pvScene.nSpirit);
        ok = PvFireDraw(dev) && ok;
    }
    FxRestore(dev, &sv);
    return ok;
}
static void PvPageNote(int lay, int f, int how)
{
    const int key = lay << 16 | f << 8 | (g_pvBackFrame == g_fxFrame ? g_pvBack + 1 : 0) << 4 | how;
    char what[160];
    if (key == g_pvNoteKey || !LogQuota(&g_pvQPage, 60)) return;
    g_pvNoteKey = key;
    if (lay == PVP_PAUSE_HEAT)
        sprintf_s(what, sizeof what, " with the pause's heat layer (%d bod%s, %d footprint(s)%s)", g_pvBodies, g_pvBodies == 1 ? "y" : "ies",
                  g_pvPrints, how == 2 ? "; added: no key shader" : "");
    else if (lay == PVP_PAUSE_SPIRIT)
        sprintf_s(what, sizeof what, " with the pause's spirit layer (%d ghost(s), %d soul fire(s), %d aura sprite(s))", g_pvGhosts, g_pvFires, g_pvAuras);
    else if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT)
        sprintf_s(what, sizeof what, " with %s", kPvPage[lay]);
    else
        what[0] = 0;
    Log("screen effects: the Effects page's preview: %s over %s%s%s", kFxName[FX_FILTER][f >= 0 && f < FX_FILTERS ? f : 0],
        g_pvBackFrame == g_fxFrame && g_pvBack >= 0 ? kPvBack[g_pvBack] : "the paused scene (the pause menu)", what,
        how == 0 && lay != PVP_NONE ? " (the layer could not be drawn)" : "");
}
// where 0: the Effects page (FxBackdropFilter, after FxPass(0)); 1: a pause frame over the frozen copy whose filter was
// changed in Options (FxBackdropFrame, after FxPass(3); the clean pause picture is its backdrop)
static void PvPageDraw(int where)
{
    void *dev = DEV;
    const int f = (int)g_fx[FX_FILTER];
    PvPageIn in;
    UINT pw = 0, ph = 0;
    int lay, how = 1;
    if (!dev) return;
    memset(&in, 0, sizeof in);
    in.thermal = f == FXF_THERMAL, in.spirit = f == FXF_GHOST;
    in.layersSeq = g_pvSeq, in.picSeq = g_fxPausePicSeq;
    in.haveHeat = (g_pvHave & PVL_HEAT) != 0, in.haveSpirit = (g_pvHave & PVL_SPIRIT) != 0;
    if (where) in.back = ShPauseTex(dev, g_fxPausePicSeq, &pw, &ph) ? PVB_PAUSE : PVB_STOCK;
    else in.back = g_pvBackFrame == g_fxFrame ? g_pvBack : PVB_STOCK;
    lay = PvPageLayer(&in);
    __try {
        if (lay == PVP_PAUSE_HEAT) how = PvLayerDraw(dev, g_pvHeat.tex, (UINT)g_pvHeat.w, (UINT)g_pvHeat.h, 1);
        else if (lay == PVP_PAUSE_SPIRIT) how = PvLayerDraw(dev, g_pvSpirit.tex, (UINT)g_pvSpirit.w, (UINT)g_pvSpirit.h, 0);
        else if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT) how = PvSceneLayer(dev, lay == PVP_SCENE_HEAT);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        how = 0;
        if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT) g_pvSceneFails = 1;
        else g_pvFails = PV_FAILS_OFF, g_pvSeq = -1, g_pvHave = 0;
        LogAnomaly("screen effects", "the Effects preview's %s faulted: off for this session", kPvPage[lay]);
    }
    if (!where) PvPageNote(lay, f, how);
}
// FxAfterPost (0x55AC29, every frame, after ShFrameEnd): the page's open / close, the pause's end (the targets stay for
// the next pause unless the address space runs low, as the Shadows page's)
static void PvFrameEnd(void)
{
    const int shown = g_fxOn && FxPageShown(), paused = FxGameplay() && (*(volatile int *)0xFD5C20 == 2 || *(volatile int *)0x12E3E2C != 0);
    if (shown != g_pvPageWas) {
        g_pvPageWas = shown;
        if (!shown) g_pvNoteKey = -1, g_pvFireFrame = -10;  // the next opening logs its preview again, a fresh fire
    }
    if (g_pvWasPaused && !paused && !shown && (g_pvHeat.tex || g_pvSpirit.tex)) {
        const int margin = ShVaMarginMb();
        if (!ShKeepTargets(margin)) {
            char why[96];
            sprintf_s(why, sizeof why, "the address space has %d MB left, under %d: made again at the next pause", margin, SH_KEEP_MARGIN_MB);
            __try {
                PvFreeAll(why);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
        } else if (!g_pvKeptLogged++) {
            LogV("screen effects: the pause ended: the preview's layers (%.1f MB) are kept for the next pause (the address space has %d MB left)",
                 ShMb(g_pvVram), margin);
        }
    }
    g_pvWasPaused = paused;
}
