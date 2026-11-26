// ================================================================ v3.43 DF343: Depth of Field (depth slices) and its page
// Request: USER_FEEDBACK_v342 ("Go ahead and attempt ... depth of field"); the decision (USER_FEEDBACK_v341 / v342):
// Refined native depth slices (depth-tested full-screen quads mark the stencil before the light grid 0x433F00
// overwrites the depth in many rooms); focus = the player's view depth (cut-scenes: the camera's target); Off /
// Cut-scenes / Always + Strength Low / High (v3.45 QS345: Low / Medium / High); its own page with a split-view preview; default Off.
// Plan: staging/v343_feedback/w4_dof/PLAN.md. Pure logic: dof_sim.h (test\dof_sim_test.c). Included by sh4windowmod.c
// after shadows.inc (screenfx.inc's device helpers: FxSave / FxBase / FxRestore, FxVtx4, the ps.1.1 token macros,
// FxDrawPic, FxPrevLoad, FxPanel, FxOnBackBuffer, FxGameplay; nothing of shadows.inc is used).
//
// The sites (byte-checked, all or nothing, listed in SH4WM_Patches):
//   0x4EFBCA `call 55DD80` in the render frame 0x4EFBC0, right after the game's own cut-scene depth of field 0x566AE0
//   and before the light grid (`call 433F00` at 0x4EFBDC; 0x55DD80 is its light count) -> `call hkDofGrid`, which runs
//   DofFrame and tail-jumps to 0x55DD80 (its eax is the caller's). Here the 3D scene, the shadow composite and its
//   stencil clear (0x560080) are done and the depth is the scene's in every room. Not a Playable Eileen hook nor in a
//   cheat-table span (ct_conflicts --span 0x4EFBCA-0x4EFBCE: none).
//   0x402E0D (the device reset's `call 401000`, shadows.inc's hook when it is in): chained, not replaced: the call
//   target found there is kept and jumped to after our render targets are released (D3D8 Reset fails while any exists).
//   (v3.43's chain at 0x526A53 for its own page is gone in v3.44: the Screen Effects page's hook calls DofFxPreview.)
// Per frame (Always, or Cut-scenes in a cut-scene; play / cut-scene phases only, the room's geometry drawn, a focus):
//   1. the back buffer copied (CopyRects) into a full-size texture of ours, then down in 2x steps (4 x 4 box each, a
//      4-tap ps.1.1 shader): half, quarter, eighth; the eighth drawn back up into a quarter-size target (tent);
//   2. the stencil's top four bits cleared, then the slices (dof_sim.h DofMakePlan) drawn as depth-tested full-screen
//      quads, colour off, REPLACE: the code of the farthest far slice the scene lies behind, or of the nearest near
//      slice it lies in front of;
//   3. for each code that blurs: its level drawn full screen with stencil EQUAL (the 4-tap tent from that level);
//   4. the top four stencil bits cleared again (nothing after 0x4EFBCA reads the stencil: its users are the shadow
//      volumes / composite and the haunting dark 0x42E400, all in the 3D pass); every state put back.
// The depth probe: once per room visit, after DOF_PROBE_AFTER eligible frames, a map of the depth against 8 fixed view
// depths is drawn into a target of ours with the game's depth buffer and read back (one GPU sync per room) on a grid
// and along the player's figure: a flat map or a figure missing from the depth turns the effect off in that room (logged).
// Off this session (logged) on any failure: a shader, a render target three times in a row, the copy, a draw, a fault.
//
// v3.44 DF344 (USER_FEEDBACK_v343 4: "Depth of Field shows in preview but not in-game"; the log: drawn in 0 of 16593
// asked frames, "no focus" every frame): the matrix buffer handed to 0x55BF50 (DrawCameraGetScreenProjView) was not
// 16-byte aligned; the game's SSE matrix multiply (Vu0MulMatrixSse, gMathMtxMul) stores with movaps, so every call
// faulted inside our __try and the player's depth was never read: no focus, nothing drawn, the pause copies without a
// depth map (the page always showed preview.dds). g_dofM is aligned now. Every frame the effect is asked for gets a
// reason (dof_sim.h DOFN_*), counted per room in the room line, the first of each kind in a room logged.
// The rows moved onto the Screen Effects page (after Otherworld Lens; sh4windowmod.c's table build calls DofBuildRows),
// its preview shows the depth of field (screenfx.inc FxBackdropFilter calls DofFxPreview); Option 3's Hide Icon is stock
// again and the separate page and its 0x526A53 chain are gone.
// v3.47 DF347 (USER_FEEDBACK_v346 2: "the DoF should be eased into, but currently has a hard starting point"): every
// blur level is reached through four thin slices a quarter level apart (dof_sim.h kDofRamp, centred on v3.46's steps);
// the composite draws each whole level opaque over the codes that reach it (stencil LESSEQUAL) and blends the next level
// over the codes between two levels at 25 / 50 / 75 % (screenfx.inc's kPsPpUp, bloom's proven up-step, SRCALPHA); the
// far side and the near side are marked and composed one after the other (the top stencil bits cleared in between). The
// pause's depth map holds a strength-free depth index (DofMakeBandList); the page marks any strength from it by alpha
// tests. Without the blend shader the steps fall back to whole levels (logged once).
// v3.52 FX352 (USER_FEEDBACK_v351 D3: "Depth of Field switches off when the camera is very close"; the log: "the player is
// nearer than 150 (a first-person view)" in 10639 frames, 7733 of them in one Subway World room): first person is the
// game's own state only (Room 302's camera: stages 1 / 0x0A, the camera dispatch 0x4FF900; the views of another place:
// the room's geometry off). A player nearer than 150 is a focus (dof_sim.h DOFF_CLOSE): on her, the far side only
// (DofPlanFarOnly), no snap between the close and the ordinary focus; the room line counts the close frames, the first
// close shot of a room is logged; the pause's preview drops the near side when its copy was taken close.
#include "dof_sim.h"

typedef HRESULT(STDMETHODCALLTYPE *DofSetRT_t)(void *, void *, void *);
typedef HRESULT(STDMETHODCALLTYPE *DofTexDesc_t)(void *, UINT, FxSurfDesc8 *);
typedef HRESULT(STDMETHODCALLTYPE *DofTexSurf_t)(void *, UINT, void **);
enum { DOFVT_IMGSURF = 27, DOFVT_COPYRECTS = 28, DOFVT_SETRT = 31, DOFVT_GETRT = 32, DOFVT_GETDS = 33 };
#define DOF_KEEP_MARGIN_MB 384   // under this much free address space our targets are freed while the effect is Off

// ---------------------------------------------------------------- settings and state
static volatile LONG g_dofMode = DOF_OFF;   // [ScreenEffects] DepthOfField
static volatile LONG g_dofStr = DOFS_MEDIUM;  // [ScreenEffects] DofStrength (v3.45 QS345: Medium = v3.44's Low, the default)
static LONG g_snapDofMode, g_snapDofStr;    // as Options opened
static int g_dofPrevOn = 1;                 // hidden [ScreenEffects] DofPreview (0: the page has no preview)
static int g_dofProbeOn = 1;                // hidden [ScreenEffects] DofProbe (0: no depth probe, every room on)
static int g_dofLogN;                       // hidden [ScreenEffects] DofLog=1: the next 120 drawn frames logged in detail
static int g_dofOn;                         // the frame hook and the reset chain are ours
static int g_dofRows;                       // v3.44 DF344: our two rows are on the Screen Effects page
static int g_dofRow0 = -1;                  // v3.44 DF344: the page row of the first (Depth of Field)
static int g_dofFailed;                     // off for the session
static const DWORD g_dofGridFn = 0x55DD80;  // the light count the hooked call made
static DWORD g_dofResetNext = 0x401000;     // the reset call's target before us
static void *g_dofDev;
static DWORD g_dofPs;                       // the 4-tap mean shader (0 = not made)
static int g_dofDsMode;                     // our targets' depth: 0 not tried, 1 none, 2 the game's
typedef struct { void *tex, *surf; int w, h; DWORD fmt; } DofRt;
static DofRt g_dofFull, g_dofLv[DOF_LEVELS + 1], g_dofUp, g_dofBand;  // g_dofLv[1..3]: half, quarter, eighth
static unsigned g_dofVram;
static int g_dofRtFails;
static volatile LONG g_dofIn;               // re-entry guard
static LARGE_INTEGER g_dofQf;
// the focus
static float g_dofF;                        // the smoothed focus (view units)
static int g_dofFocusSrc = -1;
static LONGLONG g_dofLastT;
// the pause's copies for the page: taken in the capture frame
static int g_dofCapOk, g_dofBandOk, g_dofBandCard, g_dofBandW, g_dofBandH;
static LONG g_dofCapFrame = -2;
static LONG g_dofCapSeen = -3;             // the latest capture frame ([0x12E3E2C] == 1) seen in play
static float g_dofCapF;
static int g_dofModeWas = -1, g_dofWhyWas = -1;  // the preview's last logged mode
// per room
static DWORD g_dofRoom = 0xFFFFFFFFu;
static int g_dofProbeState;                 // 0 pending, 1 on, 2 off in this room
static unsigned g_dofProbeNext = DOF_PROBE_AFTER;  // v3.45 RB345-01: the eligible frame of the room's next probe try
static int g_dofProbeTries;                 // v3.45 RB345-01: tries done in this room
static int g_dofProbeVerdict = DOFP_NONE;
static unsigned g_dofEligible, g_dofDrawn, g_dofSceneFoc;
static unsigned g_dofCloseFoc;              // v3.52 FX352: frames drawn with the player close to the camera (this room)
static int g_dofCapClose;                   // v3.52 FX352: the pause's copy was taken with a close player (the far side only)
static int g_dofCloseSeen;                  // v3.52 FX352: this room's first close shot was logged
static DofTally g_dofTally;                 // v3.44 DF344: each asked / skipped frame's reason in this room
static double g_dofMs;
static float g_dofMsMax, g_dofFMin, g_dofFMax;
static int g_dofFirstLogged;

// ---------------------------------------------------------------- texts and the rows (v3.44 DF344: on the Screen Effects page)
enum { DFM_MODE_L, DFM_STR_L, DFM_MODE_D, DFM_NA, DFM_STR_D, DFM_V0, DFM_S0 = DFM_V0 + DOF_MODE_N, DFM_N = DFM_S0 + DOFS_N };
static BYTE g_txtDof[DFM_N][72];
static DWORD g_dofModeIds[DOF_MODE_N], g_dofStrIds[DOFS_N];
typedef char DofTextsFit[DFM_N <= MSG_PP0 - MSG_DOF0 ? 1 : -1];  // v3.43: the MSG_DOF0 block (32 ids)
static const BYTE *DofText(int id)
{
    return id >= MSG_DOF0 && id < MSG_DOF0 + DFM_N ? g_txtDof[id - MSG_DOF0] : g_txtDof[DFM_NA];
}
static void DofTexts(void)
{
    int v;
    Encode(g_txtDof[DFM_MODE_L], 72, "Depth of Field");
    Encode(g_txtDof[DFM_STR_L], 72, "DoF Strength");
    Encode(g_txtDof[DFM_MODE_D], 72, "Blur by distance from the player: Off, in cut-scenes or always.");
    Encode(g_txtDof[DFM_NA], 72, "Off this session (see SH4Refined.log).");
    Encode(g_txtDof[DFM_STR_D], 72, "How strong the depth of field blur is: Low, Medium or High.");  // v3.45 QS345
    for (v = 0; v < DOF_MODE_N; ++v) {
        Encode(g_txtDof[DFM_V0 + v], 72, kDofModeName[v]);
        g_dofModeIds[v] = MSG_DOF0 + DFM_V0 + v;
    }
    for (v = 0; v < DOFS_N; ++v) {
        Encode(g_txtDof[DFM_S0 + v], 72, kDofStrName[v]);
        g_dofStrIds[v] = MSG_DOF0 + DFM_S0 + v;
    }
}
static void DofSetMode(int v, int save)
{
    if (v < 0 || v >= DOF_MODE_N) return;
    InterlockedExchange(&g_dofMode, v);
    if (save && !g_iniFallback) WritePrivateProfileStringA("ScreenEffects", "DepthOfField", kDofModeName[v], g_iniPath);
    if (g_dofLogN >= 0 && GetPrivateProfileIntA("ScreenEffects", "DofLog", 0, g_iniPath)) g_dofLogN = 120;
    LogV("depth of field: DepthOfField %s (Strength %s)%s", kDofModeName[v], kDofStrName[(unsigned)g_dofStr < DOFS_N ? (int)g_dofStr : 0],  // v3.45 QS345: was & 1
         g_dofFailed ? " (off this session: nothing is drawn)" : "");
}
static int g_dofStrMarked;  // v3.45 QS345-03: the DofStrengthV marker is on disk (read at the start or written since)
static void DofSetStr(int v, int save)
{
    if (v < 0 || v >= DOFS_N) return;
    InterlockedExchange(&g_dofStr, v);
    if (save && !g_iniFallback) {  // v3.45 RB345-02 / QS345-03: the order by DofStrWriteOrder: whatever fails, the ini left
        // on disk reads with the meaning it had (a v3.45 "Low" is never on disk without the marker, nor the marker over a
        // legacy "Low"); a failure keeps the choice for this session and is logged
        static volatile LONG s_q;
        const int order = DofStrWriteOrder(v, g_dofStrMarked);
        const char *fail = NULL;
        if (order == DOFW_VALUE) {
            if (!WritePrivateProfileStringA("ScreenEffects", "DofStrength", kDofStrName[v], g_iniPath)) fail = "the value";
        } else if (order == DOFW_VALUE_MARK) {
            if (!WritePrivateProfileStringA("ScreenEffects", "DofStrength", kDofStrName[v], g_iniPath)) fail = "the value";
            else if (WritePrivateProfileStringA("ScreenEffects", "DofStrengthV", DOF_STR_VER_S, g_iniPath)) g_dofStrMarked = 1;
        } else if (!WritePrivateProfileStringA("ScreenEffects", "DofStrengthV", DOF_STR_VER_S, g_iniPath)) {
            fail = "the marker DofStrengthV (the value not written)";
        } else if (!WritePrivateProfileStringA("ScreenEffects", "DofStrength", kDofStrName[v], g_iniPath)) {
            const BOOL back = WritePrivateProfileStringA("ScreenEffects", "DofStrengthV", NULL, g_iniPath);  // taken back
            fail = back ? "the value (the marker taken back)" : "the value, and the marker could not be taken back";
        } else {
            g_dofStrMarked = 1;
        }
        if (fail && LogQuota(&s_q, 5))
            Log("depth of field: [ScreenEffects] DofStrength=%s: %s could not be written to SH4Refined.ini (error %lu): it "
                "applies for this session only", kDofStrName[v], fail, GetLastError());
    }
    LogV("depth of field: Strength %s (DepthOfField %s)", kDofStrName[v], kDofModeName[(int)g_dofMode % DOF_MODE_N]);
}
static int __cdecl DofModeGet(void) { return (int)g_dofMode >= 0 && (int)g_dofMode < DOF_MODE_N ? (int)g_dofMode : 0; }
static int __cdecl DofStrGet(void) { return (int)g_dofStr >= 0 && (int)g_dofStr < DOFS_N ? (int)g_dofStr : 0; }
static int __cdecl DofModeIn(BYTE *cfg, int count)
{
    const int v = StepValue(DofModeGet(), count);
    (void)cfg;
    if (v >= 0) DofSetMode(v, 1);
    return 0;
}
static int __cdecl DofStrIn(BYTE *cfg, int count)
{
    const int v = StepValue(DofStrGet(), count);
    (void)cfg;
    if (v >= 0) DofSetStr(v, 1);
    return 0;
}
static int DofPreviewable(void) { return g_dofPrevOn && g_fxPrevHook; }  // v3.44 DF344: the Screen Effects page's backdrop hook
static int __cdecl DofModeDesc(void) { return MSG_DOF0 + (g_dofFailed ? DFM_NA : DFM_MODE_D); }
static int __cdecl DofStrDesc(void) { return MSG_DOF0 + (g_dofFailed ? DFM_NA : DFM_STR_D); }
// v3.44 DF344: sh4windowmod.c (the Screen Effects page's table, after the post rows): our two rows at `rows` (7 DWORDs
// each), the page's row `row0` for the first; returns how many (0: depth of field not installed)
static int DofBuildRows(DWORD *rows, int row0)
{
    g_dofRows = 0, g_dofRow0 = -1;
    if (!g_dofOn) return 0;
    DofTexts();
    FillRow(rows, DofModeIn, DofModeGet, g_dofModeIds, DOF_MODE_N, MSG_DOF0 + DFM_MODE_L, DofModeDesc);
    FillRow(rows + 7, DofStrIn, DofStrGet, g_dofStrIds, DOFS_N, MSG_DOF0 + DFM_STR_L, DofStrDesc);
    g_dofRows = 1, g_dofRow0 = row0;
    LogV("depth of field: its rows are on the Screen Effects page (rows %d and %d: Depth of Field, DoF Strength)", row0, row0 + 1);
    return 2;
}
// v3.44 DF344: v3.43's separate page is gone (its rows are on Screen Effects; Option 3's Hide Icon is stock again). The
// shared call sites in sh4windowmod.c stay (no-ops) so the merge touches as little as possible.
static int DofRowWanted(void) { return 0; }
static void DofRowInstall(DWORD *row) { (void)row; }
static int DofMenuOn(void) { return 0; }
static void DofMenuOff(const char *why) { (void)why; }
static void DofBuildPage(const DWORD *stockRows) { (void)stockRows; }
static void DofSnapshot(void) { g_snapDofMode = g_dofMode, g_snapDofStr = g_dofStr; }
static int DofChanged(void) { return g_snapOn && g_dofRows && (g_dofMode != g_snapDofMode || g_dofStr != g_snapDofStr); }
static int DofRowChanged(int row)  // row 0 = Depth of Field, 1 = DoF Strength
{
    if (!g_snapOn || !g_dofRows) return 0;
    return row == 0 ? g_dofMode != g_snapDofMode : row == 1 ? g_dofStr != g_snapDofStr : 0;
}
// v3.44 DF344: the Screen Effects page's row `row`: -1 = not one of ours, else 1 changed / 0 not
static int DofFxRowChanged(int row)
{
    if (!g_dofRows || g_dofRow0 < 0 || row < g_dofRow0 || row > g_dofRow0 + 1) return -1;
    return DofRowChanged(row - g_dofRow0);
}
static void DofDiscard(void)
{
    if (g_snapOn && g_dofMode != g_snapDofMode) DofSetMode((int)g_snapDofMode, 1);
    if (g_snapOn && g_dofStr != g_snapDofStr) DofSetStr((int)g_snapDofStr, 1);
}

// ---------------------------------------------------------------- render targets (DEFAULT pool, game thread)
static float DofMb(unsigned b) { return (float)b / (1024.0f * 1024.0f); }
static void DofRtFree(DofRt *r)
{
    const unsigned b = (unsigned)r->w * (unsigned)r->h * 4u;
    if (r->surf) ((Release_t)VT(r->surf, VT_RELEASE))(r->surf);
    if (r->tex) ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
    if (r->tex) g_dofVram = g_dofVram >= b ? g_dofVram - b : 0;
    memset(r, 0, sizeof *r);
}
static int DofRtMake(void *dev, DofRt *r, int w, int h, DWORD fmt, const char *what)
{
    HRESULT hr;
    if (r->tex && r->w == w && r->h == h && r->fmt == fmt) return 1;
    DofRtFree(r);
    hr = ((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, (UINT)w, (UINT)h, 1, 1 /*RENDERTARGET*/, fmt, 0 /*DEFAULT*/, &r->tex);
    if (FAILED(hr) || !r->tex) {
        r->tex = NULL;
        Log("depth of field: a %dx%d render target (%s, format %lu) could not be made (hr %08lX)", w, h, what, fmt, (unsigned long)hr);
        return 0;
    }
    hr = ((DofTexSurf_t)VT(r->tex, 15))(r->tex, 0, &r->surf);
    if (FAILED(hr) || !r->surf) {
        r->surf = NULL;
        ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
        r->tex = NULL;
        Log("depth of field: the %s render target's surface could not be read (hr %08lX)", what, (unsigned long)hr);
        return 0;
    }
    r->w = w, r->h = h, r->fmt = fmt;
    g_dofVram += (unsigned)w * (unsigned)h * 4u;
    LogV("depth of field: render target made: %s %dx%d (%.1f MB; ours now %.1f MB)", what, w, h, DofMb((unsigned)w * (unsigned)h * 4u),
         DofMb(g_dofVram));
    return 1;
}
static void DofFreeAll(const char *why)
{
    const unsigned had = g_dofVram;
    int k, n = (g_dofFull.tex != NULL) + (g_dofUp.tex != NULL) + (g_dofBand.tex != NULL);
    for (k = 1; k <= DOF_LEVELS; ++k) n += g_dofLv[k].tex != NULL, DofRtFree(&g_dofLv[k]);
    DofRtFree(&g_dofFull), DofRtFree(&g_dofUp), DofRtFree(&g_dofBand);
    g_dofCapOk = g_dofBandOk = g_dofBandCard = 0;
    g_dofCapFrame = -2;
    if (n) LogV("depth of field: %s: %d render target(s) released (%.1f MB)", why, n, DofMb(had));
}
static void DofForget(void)
{
    memset(&g_dofFull, 0, sizeof g_dofFull), memset(g_dofLv, 0, sizeof g_dofLv), memset(&g_dofUp, 0, sizeof g_dofUp);
    memset(&g_dofBand, 0, sizeof g_dofBand);
    g_dofVram = 0, g_dofCapOk = g_dofBandOk = g_dofBandCard = 0, g_dofCapFrame = -2;
}
static void DofFreeOld(const char *why)  // our targets of an older device: released (guarded: forgotten if already gone)
{
    __try {
        DofFreeAll(why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("depth of field", "releasing the render targets of an older Direct3D device faulted: forgotten (logged once)");
        DofForget();
    }
}
// 0x402E0D, chained: the device reset 0x402CD0 is about to release its own DEFAULT objects and Reset
static void __cdecl DofBeforeReset(void)
{
    __try {
        if (g_dofDev == DEV) DofFreeAll("device reset");
        else DofFreeOld("device reset (targets of an older device)");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("depth of field", "releasing our render targets before a device reset faulted (logged once)");
    }
}
static __declspec(naked) void hkDofBeforeReset(void)  // 402E0D: the call found there (shadows.inc's, or 401000) after ours
{
    __asm {
        pushad
        pushfd
        call DofBeforeReset
        popfd
        popad
        jmp dword ptr [g_dofResetNext]
    }
}
static void DofDevCheck(void *dev)
{
    if (dev == g_dofDev) return;
    if (g_dofDev) {
        LogV("depth of field: a new Direct3D device (%p): our objects are made again", dev);
        DofFreeOld("a new Direct3D device");
    }
    g_dofDev = dev;
    DofForget();
    g_dofPs = 0;
    g_dofDsMode = 0;
}
static int DofVaMarginMb(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    return GlobalMemoryStatusEx(&ms) ? (int)(ms.ullAvailVirtual >> 20) : 0;
}

// ---------------------------------------------------------------- the shader (ps.1.1): the mean of 4 bilinear taps, c0 = 0.5
static const DWORD kPsDofTap4[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),
    PS_END };
// v3.44 CS344-LINT-1: the start-up lint (post.inc PpInstall calls it) checks every Refined pixel shader stream against
// the forms proven through GOG's translator (post_sim.h PsLint): the screen filters and post effects (screenfx.inc
// kFxPsCode; Noir is never made), the shadow coverage / down / composite (shadows.inc, shadows_sim.h kShCompPs) and this
// 4-tap mean. Defined here, after the last of them; a new shader stream belongs in kMore.
static void RfShaderLint(void)
{
    static const struct { const DWORD *code; const char *name; } kMore[] = {
        { kPsShNorm, "shadow coverage" }, { kPsShDown, "shadow down" }, { kShCompPs, "shadow composite" },
        { kPsDofTap4, "depth of field 4-tap" } };
    const int nScreen = FXT_N - 2, nAll = nScreen + (int)(sizeof kMore / sizeof kMore[0]);  // (FXT_NONE, FXT_NOIR not made)
    char bad[400];
    int t, n = 0, nBad = 0, at, r;
    const char *why, *name;
    bad[0] = 0;
    for (t = FXT_NONE + 1; t < FXT_N + (int)(sizeof kMore / sizeof kMore[0]); ++t) {
        const DWORD *code;
        if (t == FXT_NOIR) continue;
        code = t < FXT_N ? kFxPsCode[t] : kMore[t - FXT_N].code;
        name = t < FXT_N ? kFxPsName[t] : kMore[t - FXT_N].name;
        if (!PsLint(code, 64, &at, &why)) continue;
        ++nBad;
        if (n >= (int)sizeof bad - 1) continue;  // (the count still says how many)
        r = _snprintf_s(bad + n, sizeof bad - n, _TRUNCATE, "%s%s (instruction %d: %s)", n ? ", " : "", name, at, why);
        n = r < 0 ? (int)sizeof bad - 1 : n + r;
    }
    if (nBad)
        LogAnomaly("post", "%d of %d Refined pixel shaders use forms not proven through GOG's translator (they may draw nothing): %s",
                   nBad, nAll, bad);
    else
        LogV("shaders: all %d Refined pixel shaders (%d screen filters / post effects, 3 shadow, 1 depth of field) use proven ps.1.1 "
             "forms only (PsLint; Noir is never made)", nAll, nScreen);
}
static int DofShader(void *dev)
{
    HRESULT hr;
    if (g_dofPs) return 1;
    hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kPsDofTap4, &g_dofPs);
    if (FAILED(hr) || !g_dofPs) {
        g_dofPs = 0;
        Log("depth of field: CreatePixelShader (4-tap mean) failed (hr %08lX)", (unsigned long)hr);
        return 0;
    }
    Log("depth of field: 4-tap shader made (ps.1.1, handle %lu, device %p)", g_dofPs, dev);
    return 1;
}
// v3.47 DF347: the blend draws' shader: screenfx.inc's kPsPpUp (bloom's up-step: the 4-tap mean, alpha = c2.a; proven
// live since v3.43 and linted at start). 0 = not made on this device: the composite falls back to whole levels.
static int g_dofBlendWas = -1;  // the last state logged (-1 none)
static int DofBlend(void *dev)
{
    const int ok = FxShader(dev, FXT_PPUP) && g_fxPs[FXT_PPUP] != 0;
    if (ok != g_dofBlendWas) {
        if (!ok) Log("depth of field: the blend shader (bloom's up-step) could not be made: the blur steps by whole levels here (not eased in)");
        else if (g_dofBlendWas == 0) Log("depth of field: the blend shader is there again: the blur eases in");
        g_dofBlendWas = ok;
    }
    return ok;
}
static void DofPsc(void *dev, DWORD reg, float r, float g, float b, float a)
{
    const float v[4] = { r, g, b, a };
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, reg, (void *)v, 1);
}

// ---------------------------------------------------------------- drawing helpers
static void DofRs(void *dev, DWORD s, DWORD v) { ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, s, v); }
// a quad with 4 texture coordinate sets (FVF 0x444) over [xa, xa + wq] x [ya, ya + hq] (u, v 0..1 over it), clipped to
// x in [cx0, cx1); tap k = base + (-+du, -+dv)
static HRESULT DofQuad4(void *dev, float xa, float ya, float wq, float hq, float cx0, float cx1, float du, float dv)
{
    static const float kS[4][2] = { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
    const float x0 = xa > cx0 ? xa : cx0, x1 = xa + wq < cx1 ? xa + wq : cx1;
    FxVtx4 q[4];
    int m, k;
    if (!(x1 > x0) || !(wq > 0.0f) || !(hq > 0.0f)) return S_OK;
    for (m = 0; m < 4; ++m) {
        const float x = (m & 1) ? x1 : x0, y = (m & 2) ? ya + hq : ya;
        q[m].x = x, q[m].y = y, q[m].z = 0.0f, q[m].rhw = 1.0f, q[m].c = 0xFFFFFFFFu;
        for (k = 0; k < 4; ++k) {
            q[m].t[k][0] = (x - xa) / wq + kS[k][0] * du;
            q[m].t[k][1] = (y - ya) / hq + kS[k][1] * dv;
        }
    }
    return ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(FxVtx4));
}
// a screen quad (FVF 0x144) at depth z, colour c, u v 0..1 over the whole screen [0, w) x [0, h), clipped to [cx0, cx1)
static HRESULT DofQuadZ(void *dev, float w, float h, float z, DWORD c, float cx0, float cx1)
{
    const float x0 = cx0 > 0.0f ? cx0 : 0.0f, x1 = cx1 < w ? cx1 : w;
    SpVtx q[4];
    int m;
    if (!(x1 > x0)) return S_OK;
    for (m = 0; m < 4; ++m) {
        const float x = (m & 1) ? x1 : x0, y = (m & 2) ? h : 0.0f;
        q[m].x = x - 0.5f, q[m].y = y - 0.5f, q[m].z = z, q[m].rhw = 1.0f, q[m].c = c;
        q[m].u = x / w, q[m].v = y / h;
    }
    return ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(SpVtx));
}
static void DofTex4(void *dev, void *tex)
{
    int s;
    for (s = 0; s < 4; ++s) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)s, tex);
}
static void DofPsc0(void *dev)
{
    static const float kHalf[4] = { 0.5f, 0.5f, 0.5f, 0.5f };
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, (void *)kHalf, 1);
}
// one of our targets (no depth buffer; the game's if the wrapper refuses none), its whole area as the viewport
static int DofTarget(void *dev, const DofRt *r, void *gameDs)
{
    Viewport8 vp;
    HRESULT hr = E_FAIL;
    if (g_dofDsMode != 2) {
        hr = ((DofSetRT_t)VT(dev, DOFVT_SETRT))(dev, r->surf, NULL);
        if (SUCCEEDED(hr) && !g_dofDsMode) g_dofDsMode = 1, LogV("depth of field: our render targets are drawn without a depth buffer");
    }
    if (FAILED(hr) && g_dofDsMode != 1 && gameDs) {
        const HRESULT h2 = ((DofSetRT_t)VT(dev, DOFVT_SETRT))(dev, r->surf, gameDs);
        if (SUCCEEDED(h2) && !g_dofDsMode)
            LogV("depth of field: no depth buffer refused (hr %08lX): our render targets use the game's (Z and stencil off)", (unsigned long)hr);
        if (SUCCEEDED(h2)) g_dofDsMode = 2;
        hr = h2;
    }
    if (FAILED(hr)) return 0;
    vp.x = vp.y = 0, vp.w = (DWORD)r->w, vp.h = (DWORD)r->h, vp.minZ = 0.0f, vp.maxZ = 1.0f;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
    return 1;
}
static int DofTargetBack(void *dev, void *rt, void *ds, const Viewport8 *vp)
{
    const HRESULT hr = ((DofSetRT_t)VT(dev, DOFVT_SETRT))(dev, rt, ds);
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    if (FAILED(hr)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("depth of field", "the game's render target could not be set back (hr %08lX)", (unsigned long)hr);
        return 0;
    }
    return 1;
}
static const char *const kDofLvName[DOF_LEVELS + 1] = { "", "half size", "quarter size", "eighth size" };
// the chain's targets for a w x h screen (format fmt): 1 = all there
static int DofChainReady(void *dev, int w, int h, DWORD fmt)
{
    int k;
    if (!DofRtMake(dev, &g_dofFull, w, h, fmt, "the scene copy")) return 0;
    for (k = 1; k <= DOF_LEVELS; ++k)
        if (!DofRtMake(dev, &g_dofLv[k], DofLevelDim(w, k), DofLevelDim(h, k), 21 /*A8R8G8B8*/, kDofLvName[k])) return 0;
    return DofRtMake(dev, &g_dofUp, DofLevelDim(w, 2), DofLevelDim(h, 2), 21, "the eighth's quarter-size step");
}
// down (4 x 4 box: taps at +-1 source texel) from src into dst, or up (tent: taps at +-0.5 source texel)
static int DofPass(void *dev, const DofRt *dst, void *srcTex, int srcW, int srcH, int up, void *gameDs)
{
    const float k = up ? 0.5f : 1.0f;
    if (!DofTarget(dev, dst, gameDs)) return 0;
    FxBase(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0xF);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_dofPs);
    DofPsc0(dev);
    DofTex4(dev, srcTex);
    return SUCCEEDED(DofQuad4(dev, -0.5f, -0.5f, (float)dst->w, (float)dst->h, -1e9f, 1e9f, k / (float)srcW, k / (float)srcH));
}
// the blur chain from g_dofFull (already holding the picture): 1 = done. The caller saved every state and its target.
static int DofRunChain(void *dev, void *gameDs)
{
    if (!DofPass(dev, &g_dofLv[1], g_dofFull.tex, g_dofFull.w, g_dofFull.h, 0, gameDs)) return 0;
    if (!DofPass(dev, &g_dofLv[2], g_dofLv[1].tex, g_dofLv[1].w, g_dofLv[1].h, 0, gameDs)) return 0;
    if (!DofPass(dev, &g_dofLv[3], g_dofLv[2].tex, g_dofLv[2].w, g_dofLv[2].h, 0, gameDs)) return 0;
    return DofPass(dev, &g_dofUp, g_dofLv[3].tex, g_dofLv[3].w, g_dofLv[3].h, 1, gameDs);
}
// the texture a level is composed from (level 3: its quarter-size step)
static const DofRt *DofLevelRt(int level) { return level == 3 ? &g_dofUp : level >= 1 && level <= 2 ? &g_dofLv[level] : NULL; }

// the stencil states we touch (STENCILFAIL .. STENCILWRITEMASK) and the alpha test's reference / function
static const DWORD kDofRs[] = { 53, 54, 55, 56, 57, 58, 59, 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/ };
#define DOF_NRS ((int)(sizeof kDofRs / sizeof kDofRs[0]))
static int DofSaveRs(void *dev, DWORD *v)
{
    int i;
    for (i = 0; i < DOF_NRS; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kDofRs[i], &v[i]))) return 0;
    return 1;
}
static void DofRestoreRs(void *dev, const DWORD *v)
{
    int i;
    for (i = 0; i < DOF_NRS; ++i) DofRs(dev, kDofRs[i], v[i]);
}
// untextured, no shader: stage 0 = the diffuse colour and alpha
static void DofFlatStates(void *dev)
{
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
}
// the top four stencil bits cleared over [cx0, cx1) (the game's low bits kept: write mask)
static int DofStencilClean(void *dev, float w, float h, float cx0, float cx1)
{
    DofFlatStates(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0);
    DofRs(dev, 52 /*STENCILENABLE*/, 1);
    DofRs(dev, 56 /*STENCILFUNC*/, 8 /*ALWAYS*/);
    DofRs(dev, 53 /*STENCILFAIL*/, 1 /*KEEP*/);
    DofRs(dev, 54 /*STENCILZFAIL*/, 1);
    DofRs(dev, 55 /*STENCILPASS*/, 2 /*ZERO*/);
    DofRs(dev, 59 /*STENCILWRITEMASK*/, DOF_STENCIL_MASK);
    return SUCCEEDED(DofQuadZ(dev, w, h, 0.0f, 0, cx0, cx1));
}
// DF343-001: once a stencil write may have happened, the top bits are cleared on every exit (dof_sim.h DofStGuard):
// DofStArm before the first write (the rectangle and the full-target viewport to clean with), DofStDisarm on every
// exit path (success, a failed draw, the fault handlers), on the target the marking ran on, before the state goes back
static DofStGuard g_dofSt;
static float g_dofStW, g_dofStH, g_dofStX0, g_dofStX1;
static Viewport8 g_dofStVp;
static void DofStArm(const Viewport8 *vp, float w, float h, float cx0, float cx1)
{
    g_dofStVp = *vp, g_dofStW = w, g_dofStH = h, g_dofStX0 = cx0, g_dofStX1 = cx1;
    DofStBegin(&g_dofSt);
}
static int DofStDisarm(void *dev)  // 1 = clean (or nothing to clean)
{
    if (!DofStEnd(&g_dofSt)) return 1;  // (disarmed first: a fault inside the cleanup does not run it again)
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_dofStVp);
    if (DofStencilClean(dev, g_dofStW, g_dofStH, g_dofStX0, g_dofStX1)) return 1;
    {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("depth of field", "the stencil cleanup draw failed: our top stencil bits may stay this frame");
    }
    return 0;
}
// the slices of one side of a plan into the stencil of the current target (the back buffer and the game's depth)
// v3.47 DF347: one side at a time (far: ZFUNC GREATER, near: LESS; each side's codes 1..15)
static int DofMark(void *dev, const DofSideT *pl, int side, float w, float h)
{
    int i, ok = 1;
    DofFlatStates(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0);
    DofRs(dev, 7 /*ZENABLE*/, 1);
    DofRs(dev, 14 /*ZWRITEENABLE*/, 0);
    DofRs(dev, 52 /*STENCILENABLE*/, 1);
    DofRs(dev, 56 /*STENCILFUNC*/, 8 /*ALWAYS*/);
    DofRs(dev, 53 /*STENCILFAIL*/, 1 /*KEEP*/);
    DofRs(dev, 54 /*STENCILZFAIL*/, 1 /*KEEP*/);
    DofRs(dev, 55 /*STENCILPASS*/, 3 /*REPLACE*/);
    DofRs(dev, 59 /*STENCILWRITEMASK*/, DOF_STENCIL_MASK);
    DofRs(dev, 23 /*ZFUNC*/, side == DOF_NEAR ? 2 /*LESS*/ : 5 /*GREATER*/);  // v3.47 DF347: per side
    for (i = 0; i < pl->n && ok; ++i) {
        const DofSlice *s = &pl->s[i];
        DofRs(dev, 57 /*STENCILREF*/, (DWORD)s->code << DOF_STENCIL_SHIFT);
        ok = SUCCEEDED(DofQuadZ(dev, w, h, s->z, 0, -1e9f, 1e9f));
    }
    return ok;
}
// v3.47 DF347: one side's composite (dof_sim.h DofComposeOps) over [cx0, cx1): each whole level opaque from the first
// code that reaches it (STENCILFUNC LESSEQUAL: ref <= stored), then the next level blended over each code between two
// levels at its fraction (EQUAL; kPsPpUp, SRCALPHA / INVSRCALPHA, alpha = c2.a). blend 0: whole levels only.
static int DofCompose(void *dev, const DofSideT *sd, int strength, int side, int blend, float w, float h, float cx0, float cx1, int *draws)
{
    DofOp op[DOF_OPS_MAX];
    const int n = DofComposeOps(sd, strength, side, blend, op);
    int i, ok = 1, mode = -1;
    FxBase(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 7);
    DofRs(dev, 52 /*STENCILENABLE*/, 1);
    DofRs(dev, 58 /*STENCILMASK*/, DOF_STENCIL_MASK);
    DofRs(dev, 59 /*STENCILWRITEMASK*/, 0);
    DofRs(dev, 53 /*STENCILFAIL*/, 1), DofRs(dev, 54, 1), DofRs(dev, 55, 1);  // KEEP
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    DofPsc0(dev);
    for (i = 0; i < n && ok; ++i) {
        const DofRt *r = DofLevelRt(op[i].level);
        const int blendOp = op[i].a < 1.0f;
        if (!r || !r->tex) continue;
        if (blendOp != mode) {  // the opaque draws first, then the blended ones (DofComposeOps' order)
            mode = blendOp;
            ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, blendOp ? g_fxPs[FXT_PPUP] : g_dofPs);
            DofRs(dev, 27 /*ALPHABLENDENABLE*/, (DWORD)blendOp);
            if (blendOp) {
                DofRs(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
                DofRs(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
                DofPsc(dev, 1, 1.0f, 1.0f, 1.0f, 0.0f);  // kPsPpUp: the mean x (1, 1, 1, 0) + (0, 0, 0, a)
            }
        }
        if (blendOp) DofPsc(dev, 2, 0.0f, 0.0f, 0.0f, op[i].a);
        DofRs(dev, 56 /*STENCILFUNC*/, op[i].func == DOFC_FROM ? 4 /*LESSEQUAL*/ : 3 /*EQUAL*/);
        DofRs(dev, 57 /*STENCILREF*/, (DWORD)op[i].code << DOF_STENCIL_SHIFT);
        DofTex4(dev, r->tex);
        ok = SUCCEEDED(DofQuad4(dev, -0.5f, -0.5f, w, h, cx0, cx1, 0.5f / (float)r->w, 0.5f / (float)r->h));
        if (draws) ++*draws;
    }
    DofRs(dev, 27 /*ALPHABLENDENABLE*/, 0);
    return ok;
}
// a depth map of slices (far-type, near to far) into g_dofBand with the game's depth buffer: alpha = the code x unit
// (v3.47 DF347: a slice list; the probe's codes x 16, the pause's band index x DOF_BAND_UNIT)
static int DofBandDraw(void *dev, const DofSlice *sl, int nsl, unsigned unit, void *ds, float w, float h)
{
    int i, ok;
    if (!ds || FAILED(((DofSetRT_t)VT(dev, DOFVT_SETRT))(dev, g_dofBand.surf, ds))) return 0;
    {
        Viewport8 vp;
        vp.x = vp.y = 0, vp.w = (DWORD)g_dofBand.w, vp.h = (DWORD)g_dofBand.h, vp.minZ = 0.0f, vp.maxZ = 1.0f;
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
    }
    ok = SUCCEEDED(((Clear_t)VT(dev, VT_CLEAR))(dev, 0, NULL, 1 /*TARGET*/, 0, 0.0f, 0));
    DofFlatStates(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0xF);
    DofRs(dev, 7 /*ZENABLE*/, 1);
    DofRs(dev, 14 /*ZWRITEENABLE*/, 0);
    DofRs(dev, 23 /*ZFUNC*/, 5 /*GREATER*/);  // (every map slice is far-type: the farthest passed wins)
    for (i = 0; i < nsl && ok; ++i) {
        const DofSlice *s = &sl[i];
        const unsigned a = (unsigned)s->code * unit;
        ok = SUCCEEDED(DofQuadZ(dev, w, h, s->z, (DWORD)(a > 255u ? 255u : a) << 24, -1e9f, 1e9f));
    }
    return ok;
}
// the card's sample depth (the floor plane) into g_dofBand, no depth buffer
static int DofBandCardDraw(void *dev, float w, float h, void *gameDs)
{
    int i;
    if (!DofTarget(dev, &g_dofBand, gameDs)) return 0;
    DofFlatStates(dev);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0xF);
    for (i = 0; i < 128; ++i) {  // v3.47 DF347: 128 strips (was 64), each the band index of its middle
        const float y0 = h * (float)i / 128.0f, y1 = h * (float)(i + 1) / 128.0f;
        FxRect(dev, DofBandAlpha(DofCardBand(((float)i + 0.5f) / 128.0f)) << 24, 0.0f, y0, w, y1, 0, 0, 0, 0);
    }
    return 1;
}
// the stencil marked from g_dofBand (point-sampled) over [cx0, cx1): one side's codes, each by its alpha test against
// the band index (v3.47 DF347 DofBandTest: far GREATEREQUAL, near LESSEQUAL; was EQUAL per code), in the side's order
static int DofMarkFromBand(void *dev, const DofSideT *sd, int side, float w, float h, float cx0, float cx1)
{
    int c, ok = 1;
    FxBase(dev);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    FxSampler(dev, g_dofBand.tex, 0, 1);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    DofRs(dev, 168 /*COLORWRITEENABLE*/, 0);
    DofRs(dev, 15 /*ALPHATESTENABLE*/, 1);
    DofRs(dev, 52 /*STENCILENABLE*/, 1);
    DofRs(dev, 56 /*STENCILFUNC*/, 8 /*ALWAYS*/);
    DofRs(dev, 53 /*STENCILFAIL*/, 1), DofRs(dev, 54, 1);
    DofRs(dev, 55 /*STENCILPASS*/, 3 /*REPLACE*/);
    DofRs(dev, 59 /*STENCILWRITEMASK*/, DOF_STENCIL_MASK);
    for (c = 0; c < sd->n && ok; ++c) {
        unsigned ref;
        const int f = DofBandTest(side, sd->s[c].g, &ref);
        if (!f) continue;  // (not in the band grid: the test keeps this from happening)
        DofRs(dev, 25 /*ALPHAFUNC*/, (DWORD)f);
        DofRs(dev, 24 /*ALPHAREF*/, ref);
        DofRs(dev, 57 /*STENCILREF*/, (DWORD)sd->s[c].code << DOF_STENCIL_SHIFT);
        ok = SUCCEEDED(DofQuadZ(dev, w, h, 0.0f, 0xFFFFFFFFu, cx0, cx1));
    }
    return ok;
}

// ---------------------------------------------------------------- the session's failure and the logs
static void DofFail(const char *why, HRESULT hr)
{
    if (g_dofFailed) return;
    g_dofFailed = 1;
    Log("depth of field: off for this session: %s (hr %08lX): nothing is drawn from now on", why, (unsigned long)hr);
}
// v3.44 DF344: the room line lists every reason a frame was not drawn (and the by-design skips apart)
static void DofRoomLine(void)
{
    static int s_lines;
    char why[1024], design[512];
    if (g_dofRoom == 0xFFFFFFFFu || (!g_dofEligible && !DofTallyDesign(&g_dofTally)) || s_lines >= 300) return;
    ++s_lines;
    DofTallyText(&g_dofTally, 0, why, (int)sizeof why);
    DofTallyText(&g_dofTally, 1, design, (int)sizeof design);
    {   // v3.52 FX352: + the frames drawn with the player close to the camera (after the focus range; only when any)
        char close[64];
        close[0] = 0;
        if (g_dofCloseFoc) sprintf_s(close, sizeof close, " (the player close to the camera in %u frame(s))", g_dofCloseFoc);
        LogV("depth of field: stage %lX room %lX: drawn in %u of %u frames it was asked for (not drawn: %s; the cut-scene's own focus %u); "
             "skipped by design: %s; focus %.0f .. %.0f%s; ours %.3f ms a frame on average, %.3f at most; %s %s; probe: %s; render targets %.1f MB",
             g_dofRoom >> 16, g_dofRoom & 0xFFFF, g_dofDrawn, g_dofEligible, why, g_dofSceneFoc, design,
             g_dofDrawn ? g_dofFMin : 0.0f, g_dofDrawn ? g_dofFMax : 0.0f, close, g_dofDrawn ? g_dofMs / g_dofDrawn : 0.0, g_dofMsMax,
             kDofModeName[DofModeGet()], kDofStrName[DofStrGet()], kDofProbeName[g_dofProbeVerdict], DofMb(g_dofVram));
    }
    if (g_dofEligible != DofTallyAsked(&g_dofTally)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3))
            LogAnomaly("depth of field", "stage %lX room %lX: %u asked frames but %u reasons counted", g_dofRoom >> 16, g_dofRoom & 0xFFFF,
                       g_dofEligible, DofTallyAsked(&g_dofTally));
    }
}
// v3.44 DF344: one frame's reason (the first of each kind in a room logged, at most 400 such lines a session)
static void DofNote(int why)
{
    if (DofTallyAdd(&g_dofTally, why) && g_dofRoom != 0xFFFFFFFFu) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 400))
            LogV("depth of field: stage %lX room %lX: %s: %s (DepthOfField %s)", g_dofRoom >> 16, g_dofRoom & 0xFFFF,
                 why <= DOFN_DESIGN_END ? "skipped" : "not drawn", kDofWhyNot[why], kDofModeName[DofModeGet()]);
    }
}
static void DofRoomTick(DWORD room)
{
    if (room == g_dofRoom) return;
    DofRoomLine();
    g_dofRoom = room;
    g_dofProbeState = g_dofProbeOn && !RbOff() ? 0 : 1;  // v3.45 RB345: no read back on this backend: on at once, unprobed
    g_dofProbeVerdict = g_dofProbeOn && RbOff() ? DOFP_NOREAD : DOFP_NONE;
    g_dofEligible = g_dofDrawn = g_dofSceneFoc = 0;
    g_dofCloseFoc = 0, g_dofCloseSeen = 0;  // v3.52 FX352
    g_dofProbeNext = DOF_PROBE_AFTER, g_dofProbeTries = 0;  // v3.45 RB345-01
    memset(&g_dofTally, 0, sizeof g_dofTally);
    g_dofMs = 0.0;
    g_dofMsMax = 0.0f;
    g_dofFMin = 1e9f, g_dofFMax = 0.0f;
    g_dofF = 0.0f;  // a new room: the focus snaps
}

// ---------------------------------------------------------------- the focus: the player's view depth and figure
// v3.44 DF344: 16-byte aligned (the root cause of v3.43's "never in play"): 0x55BF50's matrix multiply (Vu0MulMatrixSse)
// stores into it with movaps; unaligned, every call faulted inside DofPlayer's __try and no focus was ever found
static __declspec(align(16)) float g_dofM[16];  // world -> GS (0x55BF50), this frame
#define DOF_FIG_H 170.0f  // v3.44 DF344: her figure's height (was 900: the samples ran far above her head)
static void DofXform(const float p[3], float h[4])
{
    int j;
    for (j = 0; j < 4; ++j) h[j] = p[0] * g_dofM[j] + p[1] * g_dofM[4 + j] + p[2] * g_dofM[8 + j] + g_dofM[12 + j];
}
static int DofScreenPt(const float h[4], const Viewport8 *vp, float *x, float *y)  // screenfx.inc GvScreen's mapping
{
    float gx, gy, cx, cy;
    if (!(h[3] > 1.0f)) return 0;
    gx = h[0] / h[3], gy = h[1] / h[3];
    if (!_finite(gx) || !_finite(gy)) return 0;
    if (g_haveConsts && g_native.c2[0] != 0.0f && g_native.c2[1] != 0.0f) {
        cx = gx * g_native.c2[0] + g_native.c3[0];
        cy = gy * g_native.c2[1] + g_native.c3[1];
    } else {
        cx = (gx - 2048.0f) / 256.0f;
        cy = -(gy - 2048.0f) / 224.0f;
    }
    *x = (float)vp->x + (cx + 1.0f) * 0.5f * (float)vp->w;
    *y = (float)vp->y + (1.0f - cy) * 0.5f * (float)vp->h;
    return _finite(*x) && _finite(*y);
}
// the player's view depth (*w) and her figure on screen (feet and head points; *figOk 0 = not on screen). 1 = a depth.
static int DofPlayer(const Viewport8 *vp, float *w, float fig[2][2], int *figOk)
{
    float p[3], h[4], a[4], b[4];
    *figOk = 0;
    *w = 0.0f;
    __try {
        memcpy(p, (const void *)0x10A0F60, 12);
        ((void(__cdecl *)(float *))0x55BF50)(g_dofM);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static volatile LONG s_q;  // v3.44 DF344: was silent (v3.43's every frame)
        if (LogQuota(&s_q, 3))
            LogAnomaly("depth of field", "reading the player's position / the camera matrix (0x55BF50, buffer %p) faulted: no focus", (void *)g_dofM);
        return 0;
    }
    if (!_finite(p[0] + p[1] + p[2])) return 0;
    DofXform(p, h);
    if (!_finite(h[3])) return 0;
    *w = h[3];
    {   // her figure: the feet and DOF_FIG_H along the vertical, whichever side projects above the feet (sign-agnostic)
        float q[3] = { p[0], p[1] - DOF_FIG_H, p[2] }, r[3] = { p[0], p[1] + DOF_FIG_H, p[2] };
        float fx, fy, ax, ay, bx, by;
        DofXform(q, a), DofXform(r, b);
        if (DofScreenPt(h, vp, &fx, &fy)) {
            const int oa = DofScreenPt(a, vp, &ax, &ay), ob = DofScreenPt(b, vp, &bx, &by);
            if (oa || ob) {
                const int useA = oa && (!ob || ay < by);
                fig[0][0] = fx, fig[0][1] = fy;
                fig[1][0] = useA ? ax : bx, fig[1][1] = useA ? ay : by;
                *figOk = 1;
            }
        }
    }
    return 1;
}

// ---------------------------------------------------------------- the depth probe (once per room visit)
static int DofProbeRun(void *dev, void *ds, int w, int h, float p22, float p32, float F, const float fig[2][2], int figOk,
                       const Viewport8 *vp)
{
    DofSliceList pl;  // v3.47 DF347: a slice list (was a plan)
    DofProbeIn in;
    RbRead rb;  // v3.45 RB345: post.inc's read back (was one CopyRects into an image surface: failed under dgVoodoo2 D3D11)
    FxLockRect8 lr;
    LARGE_INTEGER t0, t1;
    int j, verdict = DOFP_NONE;
    float share = 0.0f;
    char codes[64];
    if (RbOff()) return DOFP_NOREAD;  // v3.45 RB345: no read back on this backend: no map drawn (the room line says so)
    memset(&rb, 0, sizeof rb);
    rb.path = -1;
    QueryPerformanceCounter(&t0);
    memset(&pl, 0, sizeof pl);
    memset(&in, 0, sizeof in);
    in.figMin = -1;
    for (j = 0; j < DOF_PROBE_N; ++j) {  // the probe's slices: fixed view depths, near to far, code j + 1
        const float z = DofDepthAt(kDofProbeW[j], p22, p32);
        if (!(z > 0.0f && z < 1.0f)) continue;
        pl.s[pl.n].code = j + 1, pl.s[pl.n].g = 0, pl.s[pl.n].w = kDofProbeW[j], pl.s[pl.n].z = z;
        ++pl.n;
    }
    if (!pl.n || !DofRtMake(dev, &g_dofBand, w, h, 21, "the depth map")) return DOFP_NONE;
    g_dofBandOk = g_dofBandCard = 0;  // (the pause's depth map, if any, is gone)
    if (!DofBandDraw(dev, pl.s, pl.n, 16u, ds, (float)w, (float)h)) {  // (alpha = code x 16, read back / 16)
        static int s_logged;
        if (!s_logged++) Log("depth of field: the depth probe could not draw with the game's depth buffer (multisampled?): every room on");
        return DOFP_NONE;
    }
    __try {
        if (!RbBegin(dev, g_dofBand.surf, (UINT)w, (UINT)h, &rb, "the depth probe")) {
            verdict = DOFP_NOREAD;  // v3.45 RB345: honest: nothing read (RbBegin logged the session's verdict once)
        } else {
            int gx, gy, k;
            lr = rb.lr;
            for (gy = 0; gy < DOF_PROBE_GY; ++gy)
                for (gx = 0; gx < DOF_PROBE_GX; ++gx) {
                    const int x = (int)(((float)gx + 0.5f) * (float)w / DOF_PROBE_GX), y = (int)(((float)gy + 0.5f) * (float)h / DOF_PROBE_GY);
                    const DWORD px = *(const DWORD *)(lr.bits + (size_t)y * (size_t)lr.pitch + (size_t)x * 4u);
                    const int c = (int)(px >> 24) / 16;
                    if (c >= 0 && c <= DOF_PROBE_N) ++in.hist[c], ++in.n;
                }
            if (figOk)
                for (k = 0; k < DOF_PROBE_FIG; ++k) {
                    const float t = (float)k / (float)(DOF_PROBE_FIG - 1);
                    const int x = (int)(fig[0][0] + (fig[1][0] - fig[0][0]) * t), y = (int)(fig[0][1] + (fig[1][1] - fig[0][1]) * t);
                    if (x >= (int)vp->x && y >= (int)vp->y && x < (int)(vp->x + vp->w) && y < (int)(vp->y + vp->h) && x < w && y < h && x >= 0 && y >= 0) {
                        const int c = (int)(*(const DWORD *)(lr.bits + (size_t)y * (size_t)lr.pitch + (size_t)x * 4u) >> 24) / 16;
                        if (in.figMin < 0 || c < in.figMin) in.figMin = c;
                        ++in.figN;
                    }
                }
            RbEnd(&rb);
            verdict = DofProbeVerdict(&in, F, &share);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (rb.owned) Interp_OwnEnd();
        rb.owned = 0;
        __try {
            RbEnd(&rb);  // v3.45 RB345: unlock and release
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        verdict = DOFP_NONE;
    }
    QueryPerformanceCounter(&t1);
    {
        int o = 0;
        codes[0] = 0;
        for (j = 0; j <= DOF_PROBE_N && o < (int)sizeof codes - 8; ++j) o += sprintf_s(codes + o, sizeof codes - (size_t)o, "%s%u", j ? " " : "", in.hist[j]);
    }
    {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 400))
            LogV("depth of field: stage %lX room %lX: depth probe (%dx%d, %u samples, %.2f ms, %s%s): codes %s (the largest %.0f %%); her figure "
                 "%d sample(s), nearest code %d (her depth %.0f = code %d): %s", g_dofRoom >> 16, g_dofRoom & 0xFFFF, w, h, in.n,
                 g_dofQf.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)g_dofQf.QuadPart : 0.0,
                 rb.path >= 0 ? "read back ok via the " : "no read back", rb.path >= 0 ? kRbPathShort[rb.path] : "",  // v3.45 RB345
                 codes, share * 100.0f, in.figN, in.figMin, F, DofProbeCodeOf(F), kDofProbeName[verdict]);
    }
    return verdict;
}

// ---------------------------------------------------------------- the frame (0x4EFBCA, before the light grid)
// DF343-001b: a path's saved device state (the frame / probe: g_dofCtx; the page backdrop and preview: g_dofPv), put
// back once on every exit, the fault handlers included (dof_sim.h DofRestorePlan: target, stencil cleanup, states,
// viewport, release)
typedef struct { int saved, rsSaved; FxSaved sv; Viewport8 vp; void *rt, *ds; DWORD rs[DOF_NRS]; } DofCtx;
static DofCtx g_dofCtx, g_dofPv;
// the viewport, every FxSave state (npsc pixel constants), the stencil / alpha-test states, the target and depth-stencil
// (AddRef'd). 0 = nothing saved (the caller skips); else restore with DofCtxRestoreP on every exit.
static int DofCtxSave(void *dev, DofCtx *c, int npsc)
{
    c->saved = c->rsSaved = 0;
    c->rt = c->ds = NULL;
    if (FAILED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &c->vp))) return 0;
    if (!FxSave(dev, &c->sv, npsc)) return 0;  // (FxSave logs a failed query once)
    c->saved = 1;
    c->rsSaved = DofSaveRs(dev, c->rs);
    if (FAILED(((GetSurf_t)VT(dev, DOFVT_GETRT))(dev, &c->rt)) || !c->rt) c->rt = NULL;
    if (FAILED(((GetSurf_t)VT(dev, DOFVT_GETDS))(dev, &c->ds))) c->ds = NULL;
    return 1;
}
static void DofCtxRestoreP(void *dev, DofCtx *c)
{
    DofRestoreIn f;
    int st[DOFR_N], n, i;
    void *rt = c->rt, *ds = c->ds;
    const int wasSaved = c->saved, wasRs = c->rsSaved;
    f.saved = c->saved, f.rsSaved = c->rsSaved, f.held = rt != NULL, f.dirty = g_dofSt.dirty;
    n = DofRestorePlan(&f, st);
    c->saved = c->rsSaved = 0, c->rt = c->ds = NULL;  // (cleared first: restore once, even if a step below faults)
    for (i = 0; i < n; ++i)
        switch (st[i]) {
        case DOFR_TARGET: ((DofSetRT_t)VT(dev, DOFVT_SETRT))(dev, rt, ds); break;
        case DOFR_CLEAN: DofStDisarm(dev); break;
        case DOFR_STATE:
            if (wasSaved) FxRestore(dev, &c->sv);  // (FxRestore also releases what FxSave AddRef'd: only once)
            if (wasRs) DofRestoreRs(dev, c->rs);
            break;
        case DOFR_VIEWPORT: ((Viewport_t)VT(dev, FXV_SETVP))(dev, &c->vp); break;
        case DOFR_RELEASE:
            ((Release_t)VT(rt, VT_RELEASE))(rt);
            if (ds) ((Release_t)VT(ds, VT_RELEASE))(ds);
            break;
        }
    g_dofSt.dirty = 0;  // (no target held: nothing could be cleaned)
}
static void DofCtxRestore(void *dev) { DofCtxRestoreP(dev, &g_dofCtx); }
// a fault handler's restore: guarded (a fault inside it leaves the rest undone but never loops)
static void DofCtxRescue(void *dev, DofCtx *c)
{
    __try {
        DofCtxRestoreP(dev, c);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("depth of field", "restoring the device state after a fault faulted too");
    }
    g_dofSt.dirty = 0;
}
// v3.47 DF347: the stencil cleared, then each side of the plan that has slices marked (from the depth, or fromBand: from
// g_dofBand's index) and composed, the top bits cleared again between the sides. The caller armed the stencil guard
// (DofStArm) and disarms it after (the last clean). 1 = every draw went through.
static int DofDrawSides(void *dev, const DofPlanT *pl, int strength, int fromBand, float w, float h, int *draws)
{
    const int blend = DofBlend(dev);
    int sd, ok = DofStencilClean(dev, w, h, -1e9f, 1e9f), done = 0;
    for (sd = 0; sd < DOF_SIDES && ok; ++sd) {
        const DofSideT *s = &pl->side[sd];
        if (!s->n) continue;
        if (done++) ok = DofStencilClean(dev, w, h, -1e9f, 1e9f);
        ok = ok && (fromBand ? DofMarkFromBand(dev, s, sd, w, h, -1e9f, 1e9f) : DofMark(dev, s, sd, w, h)) &&
             DofCompose(dev, s, strength, sd, blend, w, h, -1e9f, 1e9f, draws);
    }
    return ok;
}
// v3.44 DF344: *drew = DOFN_DRAWN when the depth of field was drawn, else the reason (each early exit used to be silent)
// v3.52 FX352: farOnly = the player is close to the camera (DOFF_CLOSE): the plan's near side is dropped (DofPlanFarOnly)
static int DofDrawFrame(void *dev, int draw, int cap, float F, int strength, int farOnly, int *nSlices, int *nDraws, const char **fail,
                        int *drew)
{
    FxSurfDesc8 d;
    DofPlanT pl;
    DofSliceList all;  // v3.47 DF347: the pause's depth map (the band grid), was High's plan
    float p22, p32, w, h;
    int ok = 1;
    *fail = NULL;
    *drew = DOFN_NOPLAN;
    __try {
        p22 = *(volatile float *)(0x12E3ED0 + 4 * 10), p32 = *(volatile float *)(0x12E3ED0 + 4 * 14);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        p22 = p32 = 0.0f;
    }
    // (no plan: `draw` is this function's copy; the caller's stays 1 and tallies *drew = DOFN_NOPLAN, set above, on both the
    // early return below and the capture path: DofFrame's DofNote(draw ? drew : why). Codex inspection 4, DF344-TALLY-1)
    if (draw && !DofMakePlan(F, p22, p32, strength, &pl)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("depth of field", "no slices for focus %.0f (projection z column %g %g): not drawn", F, p22, p32);
        draw = 0;
    } else if (draw && farOnly && !DofPlanFarOnly(&pl, strength)) {  // v3.52 FX352: nothing beyond her within the far plane
        draw = 0;  // (*drew stays DOFN_NOPLAN: "no slice between the near and far planes"; not an anomaly)
    }
    if (!draw && !cap) return 1;
    if (!DofShader(dev)) return *fail = "the 4-tap shader could not be made", 0;
    if (!DofCtxSave(dev, &g_dofCtx, 3)) {  // DF343-001b (a failed query: skipped; restored on every exit from here); v3.47 DF347: c0..c2
        if (draw) *drew = DOFN_NOSTATE;
        return 1;
    }
    if (g_dofCtx.rt && !g_dofCtx.ds) {  // no depth-stencil bound in this frame: nothing to slice; skipped (not a failure)
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("depth of field", "no depth-stencil surface at 004EFBCA (frame %ld): skipped", g_fxFrame);
        DofCtxRestore(dev);
        if (draw) *drew = DOFN_NODS;
        return 1;
    }
    if (!g_dofCtx.rsSaved || !g_dofCtx.rt || !g_dofCtx.ds || FAILED(((FxGetDesc_t)VT(g_dofCtx.rt, 8))(g_dofCtx.rt, &d)) ||
        (d.fmt != 21 && d.fmt != 22) || d.w < 64 || d.h < 64) {
        *fail = !g_dofCtx.rsSaved ? "a stencil state could not be read" : !g_dofCtx.rt ? "no render target" :
                !g_dofCtx.ds ? "no depth-stencil surface" : "the back buffer is not a 32-bit surface";
        DofCtxRestore(dev);
        return 0;
    }
    w = (float)d.w, h = (float)d.h;
    if (!DofChainReady(dev, (int)d.w, (int)d.h, d.fmt)) {
        DofCtxRestore(dev);
        if (draw) *drew = DOFN_NORT;
        if (++g_dofRtFails < 3) return 1;  // a full video memory for a moment: not drawn this frame, tried again
        return *fail = "our render targets could not be made three times in a row", 0;
    }
    g_dofRtFails = 0;
    if (FAILED(((FxCopyRects_t)VT(dev, DOFVT_COPYRECTS))(dev, g_dofCtx.rt, NULL, 0, g_dofFull.surf, NULL))) {
        DofCtxRestore(dev);
        return *fail = "the scene could not be copied (CopyRects)", 0;
    }
    if (!cap) g_dofCapOk = 0;  // the scene copy and its levels now hold this frame, not the pause's
    ok = DofRunChain(dev, g_dofCtx.ds);
    if (ok && cap) {  // the paused scene for the page: the copy (clean) and its depth map at this focus
        g_dofCapOk = 1, g_dofCapFrame = g_fxFrame, g_dofCapF = F, g_dofBandOk = g_dofBandCard = 0;
        g_dofCapClose = farOnly;  // v3.52 FX352: the page's preview of this pause drops the near side too
        if (F > 0.0f && DofMakeBandList(F, p22, p32, &all) && DofRtMake(dev, &g_dofBand, (int)d.w, (int)d.h, 21, "the depth map") &&
            DofBandDraw(dev, all.s, all.n, DOF_BAND_UNIT, g_dofCtx.ds, w, h))  // v3.47 DF347: the band grid's index x 8
            g_dofBandOk = 1, g_dofBandW = (int)d.w, g_dofBandH = (int)d.h;
        LogV("depth of field: the paused scene kept for the Screen Effects preview: %dx%d copy + blur levels%s (focus %.0f%s, frame %ld)", (int)d.w,
             (int)d.h, g_dofBandOk ? " + its depth map" : " (no depth map: the page shows preview.dds with a sample depth)", F,
             farOnly ? ", the player close to the camera: the far side only" : "", g_dofCapFrame);  // v3.52 FX352
    }
    if (!DofTargetBack(dev, g_dofCtx.rt, g_dofCtx.ds, &g_dofCtx.sv.vp)) ok = 0;
    if (ok && draw) {
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_dofCtx.sv.vp);
        *nSlices = pl.n;
        DofStArm(&g_dofCtx.sv.vp, w, h, -1e9f, 1e9f);  // DF343-001: cleaned below on every exit, failures included
        ok = DofDrawSides(dev, &pl, strength, 0, w, h, nDraws);  // v3.47 DF347: far, then near
        if (!DofStDisarm(dev)) ok = 0;
        if (ok) *drew = DOFN_DRAWN;
    }
    DofCtxRestore(dev);
    if (!ok) return *fail = "a draw failed", 0;
    return 1;
}
static void DofFrame(void *dev)
{
    const int phase = *(volatile int *)0xFD5C20, capture = *(volatile int *)0x12E3E2C == 1, play = FxGameplay();
    const int mode = DofModeGet(), strength = DofStrGet();
    const DWORD room = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
    int gate, cap, draw = 0, src = DOFF_NONE, figOk = 0, nSlices = 0, nDraws = 0, why = DOFN_DRAWN, drew = DOFN_NOPLAN;
    float F = 0.0f, pw = 0.0f, fig[2][2], dt;
    Viewport8 vp;
    LARGE_INTEGER t0, t1;
    const char *fail = NULL;
    if (!g_dofQf.QuadPart) QueryPerformanceFrequency(&g_dofQf);
    QueryPerformanceCounter(&t0);
    dt = g_dofLastT && g_dofQf.QuadPart ? (float)((double)(t0.QuadPart - g_dofLastT) / (double)g_dofQf.QuadPart) : 0.0f;
    g_dofLastT = t0.QuadPart;
    if (capture && play) g_dofCapSeen = g_fxFrame;  // the pause picture's frame: our copies are this pause's only if taken in it
    gate = DofGate(mode, play, phase, *(volatile int *)0xFD6C78 != 0, DofIsRoom302(room >> 16));
    cap = capture && play && (phase == 0 || phase == 1) && g_dofRows && DofPreviewable() && g_dofCapFrame != g_fxFrame;
    if (play) DofRoomTick(room);
    if (gate == DOFG_OFF && !cap) {  // Off: our targets go when the address space runs low (else kept for the next use)
        if ((g_dofFull.tex || g_dofBand.tex) && !FxPageShown() && phase != 2 && DofVaMarginMb() < DOF_KEEP_MARGIN_MB) {
            char whyFree[96];
            sprintf_s(whyFree, sizeof whyFree, "Off and the address space has %d MB left", DofVaMarginMb());
            DofFreeAll(whyFree);
        }
        return;
    }
    if (gate != DOFG_ON && play && DofGateWhyNot(gate) > 0) DofNote(DofGateWhyNot(gate));  // v3.44 DF344: by design, counted
    if (gate != DOFG_ON && !cap) return;
    if (g_dofFailed || !FxOnBackBuffer(dev)) {
        if (gate == DOFG_ON) ++g_dofEligible, DofNote(g_dofFailed ? DOFN_FAILED : DOFN_NOTBACK);  // v3.44 DF344 (was silent)
        return;
    }
    DofDevCheck(dev);
    if (FAILED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &vp))) {
        if (gate == DOFG_ON) ++g_dofEligible, DofNote(DOFN_NOVP);
        return;
    }
    {
        const int sceneOn = *(volatile int *)0x137FA1C != 0 && *(volatile int *)0x137FA10 != 0;
        const float sceneF = *(volatile float *)0x137FA04;  // 0x566A10's centre (DofState.dist[2] at 0x137F9E8 + 0x1C)
        const int playerOk = DofPlayer(&vp, &pw, fig, &figOk);
        float Ft = 0.0f;
        src = DofPickFocus(phase, sceneOn, sceneF, playerOk && (figOk || phase == 0), pw, &Ft);
        if (src != DOFF_NONE) {
            g_dofF = DofFocusStep(g_dofF, Ft, dt, !DofSameSubject(src, g_dofFocusSrc));  // v3.52 FX352: player <-> close: no snap
            F = g_dofF;
        } else {
            why = DofNoFocusWhy(playerOk, pw, figOk, phase);
        }
        if (!DofSameSubject(src, g_dofFocusSrc)) {  // v3.52 FX352: (was src != g_dofFocusSrc)
            static volatile LONG s_q;
            if (LogQuota(&s_q, 200))
                LogV("depth of field: stage %lX room %lX: focus %s (%.0f)%s%s (the player's view depth %.0f)", room >> 16, room & 0xFFFF,
                     kDofFocusName[src], Ft, src == DOFF_NONE ? ": " : "", src == DOFF_NONE ? kDofWhyNot[why] : "", pw);
        }
        if (src == DOFF_CLOSE && !g_dofCloseSeen) {  // v3.52 FX352: the first close shot of a room
            static volatile LONG s_q;
            g_dofCloseSeen = 1;
            if (LogQuota(&s_q, 300))
                LogV("depth of field: stage %lX room %lX: the player is close to the camera (view depth %.0f): the focus on the player "
                     "(%.0f), the far side only (v3.51 took this for a first-person view and drew nothing)", room >> 16, room & 0xFFFF, pw, F);
        }
        g_dofFocusSrc = src;
    }
    if (gate == DOFG_ON) {
        ++g_dofEligible;
        if (src != DOFF_NONE && g_dofProbeState == 0) {  // the room's probe, after its load settled; nothing drawn until it answered
            if (g_dofEligible >= g_dofProbeNext && !capture) {  // v3.45 RB345-01: a retry waits DOF_PROBE_AFTER more
                FxSurfDesc8 d;
                int v = DOFP_NONE;
                float p22 = *(volatile float *)(0x12E3ED0 + 4 * 10), p32 = *(volatile float *)(0x12E3ED0 + 4 * 14);
                if (DofCtxSave(dev, &g_dofCtx, 0)) {  // DF343-001b: restored here, or by DofFrameC's handler on a fault
                    if (g_dofCtx.rt && g_dofCtx.ds && SUCCEEDED(((FxGetDesc_t)VT(g_dofCtx.rt, 8))(g_dofCtx.rt, &d)))
                        v = DofProbeRun(dev, g_dofCtx.ds, (int)d.w, (int)d.h, p22, p32, F, (const float(*)[2])fig, figOk, &vp);
                    DofCtxRestore(dev);
                }
                if (DofProbeRetry(v, RbOff(), ++g_dofProbeTries)) {  // v3.45 RB345-01: nothing read this time: pending
                    g_dofProbeNext = g_dofEligible + DOF_PROBE_AFTER;
                    LogT("depth of field: the room's depth probe read nothing (try %d of %d; the read back is not given up): "
                         "tried again in %d frames", g_dofProbeTries, DOF_PROBE_TRIES, DOF_PROBE_AFTER);
                } else {
                    g_dofProbeVerdict = v;
                    g_dofProbeState = v == DOFP_FLAT || v == DOFP_PLAYER ? 2 : 1;
                }
            }
        }
        if (src != DOFF_NONE) why = g_dofProbeState == 0 ? DOFN_PROBEWAIT : g_dofProbeState == 2 ? DOFN_PROBEOFF : DOFN_DRAWN;
        draw = src != DOFF_NONE && g_dofProbeState == 1;
    }
    if (!draw && !cap) {
        if (gate == DOFG_ON) DofNote(why);
        return;
    }
    if (!DofDrawFrame(dev, draw, cap, F, strength, src == DOFF_CLOSE, &nSlices, &nDraws, &fail, &drew)) {  // v3.52 FX352: + farOnly
        if (gate == DOFG_ON) DofNote(DOFN_FAILED);
        DofFail(fail ? fail : "a draw failed", E_FAIL);
        return;
    }
    QueryPerformanceCounter(&t1);
    if (gate == DOFG_ON) DofNote(draw ? drew : why);
    if (draw && drew == DOFN_DRAWN) {
        const float ms = g_dofQf.QuadPart ? (float)((double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)g_dofQf.QuadPart) : 0.0f;
        ++g_dofDrawn;
        g_dofMs += ms;
        if (ms > g_dofMsMax) g_dofMsMax = ms;
        if (F < g_dofFMin) g_dofFMin = F;
        if (F > g_dofFMax) g_dofFMax = F;
        if (src == DOFF_SCENE) ++g_dofSceneFoc;
        if (src == DOFF_CLOSE) ++g_dofCloseFoc;  // v3.52 FX352
        if (!g_dofFirstLogged++)
            Log("depth of field: first frame drawn (%s, %s): focus %.0f (%s), %d slice(s), %d blurred band(s), %.2f ms; render targets %.1f MB; %s",
                kDofModeName[mode], kDofStrName[strength], F, kDofFocusName[src], nSlices, nDraws, ms, DofMb(g_dofVram),
                g_dofBlendWas > 0 ? "eased in (a quarter level a step)" : "whole levels (no blend shader: not eased in)");  // v3.47 DF347
        if (g_dofLogN > 0) {
            --g_dofLogN;
            LogV("depth of field: frame %ld: focus %.0f (%s, target %.0f), %d slice(s), %d band(s), %.3f ms, phase %d", g_fxFrame, F,
                 kDofFocusName[src], pw, nSlices, nDraws, ms, phase);
        }
    }
}
static void __cdecl DofFrameC(void)
{
    void *dev = DEV;
    if (!g_dofOn || !dev) return;
    if (InterlockedExchange(&g_dofIn, 1)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("depth of field", "the frame hook was entered again while it ran: skipped");
        return;
    }
    BgPush(BGC_DOF);
    __try {
        DofFrame(dev);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DofCtxRescue(dev, &g_dofCtx);  // DF343-001/001b: the saved target, the stencil cleanup, the states, before off
        DofFail("a fault inside the frame", E_FAIL);
    }
    BgPop();
    InterlockedExchange(&g_dofIn, 0);
}
static __declspec(naked) void hkDofGrid(void)  // 4EFBCA: was `call 55DD80` (no arguments; the caller tests its eax)
{
    __asm {
        pushad
        pushfd
        call DofFrameC
        popfd
        popad
        jmp dword ptr [g_dofGridFn]
    }
}

// ---------------------------------------------------------------- the Screen Effects page's preview (v3.44 DF344)
// screenfx.inc FxBackdropFilter, on the Screen Effects page, right after its backdrop (the paused scene or preview.dds /
// the stock hallway) and before bloom / grading / the lens, the noise and the filter (the game's own order: our frame
// hook runs before all of them). Over the whole picture (no split line: as the page's other effects): the picture on
// screen is copied and blurred; where it blurs comes from the pause's depth map when the picture is that pause
// (g_fxPrevPause) and the pause had one, else from the sample depth (the floor plane: far at the top, near at the bottom).
static int DofMode(int *why)
{
    DofPrevIn in;
    in.previewOn = g_dofPrevOn;
    in.hook = g_fxPrevHook;
    in.failed = g_dofFailed;
    in.pausedGame = FxGameplay() && *(volatile int *)0xFD5C20 == 2 && g_fxPrevPause;
    in.haveCopy = g_dofCapOk && g_dofCapFrame == g_dofCapSeen && g_dofFull.tex && g_dofUp.tex && g_dofFull.w == (int)FxW() && g_dofFull.h == (int)FxH();
    in.haveDepth = g_dofBandOk && g_dofBand.tex && g_dofBandW == g_dofFull.w && g_dofBandH == g_dofFull.h;
    return DofPreviewMode(&in, why);
}
static void DofFxPreview(void)
{
    static int s_was = 0;
    void *dev = DEV;
    int why, mode, ok = 1, draws = 0;
    const int on = g_dofRows && DofModeGet() != DOF_OFF;
    if (!on || !dev || !FxOnBackBuffer(dev)) {
        if (s_was) s_was = 0, g_dofModeWas = g_dofWhyWas = -1;
        return;
    }
    s_was = 1;
    mode = DofMode(&why);
    if (mode != g_dofModeWas || why != g_dofWhyWas) {
        LogV("depth of field: the Screen Effects preview: %s (%s)", mode == DOFV_SCENE ? "the paused scene with its depth map" :
             mode == DOFV_CARD ? "the picture with a sample depth" : "no depth of field", kDofWhy[why]);
        g_dofModeWas = mode, g_dofWhyWas = why;
    }
    if (mode == DOFV_NONE) return;
    DofDevCheck(dev);
    BgPush(BGC_DOF);
    __try {
        FxSurfDesc8 d;
        const int strength = DofStrGet();
        if (DofCtxSave(dev, &g_dofPv, 3)) {  // DF343-001b: put back below, or by the handler on a fault; v3.47 DF347: c0..c2
            void *const rt = g_dofPv.rt, *const ds = g_dofPv.ds;
            float w, h;
            if (g_dofPv.rsSaved && rt && SUCCEEDED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) && (d.fmt == 21 || d.fmt == 22) && d.w >= 64 && d.h >= 64) {
                w = (float)d.w, h = (float)d.h;
                if (ds && DofShader(dev)) {
                    DofPlanT pl;  // v3.47 DF347: every code of the strength, marked from the depth map's index
                    DofPlanCodes(strength, &pl);
                    if (mode == DOFV_SCENE && g_dofCapClose) DofPlanFarOnly(&pl, strength);  // v3.52 FX352: as in play
                    // the picture on screen (the pause's own picture, filtered or not, is what the page shows) into the chain
                    ok = DofChainReady(dev, (int)d.w, (int)d.h, d.fmt) &&
                         SUCCEEDED(((FxCopyRects_t)VT(dev, DOFVT_COPYRECTS))(dev, rt, NULL, 0, g_dofFull.surf, NULL)) && DofRunChain(dev, ds);
                    if (mode == DOFV_CARD) {  // the sample depth
                        g_dofCapOk = g_dofBandOk = 0;  // (the scene copy now holds the page's picture)
                        if (ok && (!g_dofBandCard || g_dofBandW != (int)d.w || g_dofBandH != (int)d.h)) {
                            ok = DofRtMake(dev, &g_dofBand, (int)d.w, (int)d.h, 21, "the depth map") && DofBandCardDraw(dev, w, h, ds);
                            if (ok) g_dofBandCard = 1, g_dofBandW = (int)d.w, g_dofBandH = (int)d.h;
                        }
                    }
                    if (!DofTargetBack(dev, rt, ds, &g_dofPv.vp)) ok = 0;
                    if (ok) {
                        ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_dofPv.sv.vp);
                        DofStArm(&g_dofPv.sv.vp, w, h, -1e9f, 1e9f);  // DF343-001: cleaned on every exit
                        ok = DofDrawSides(dev, &pl, strength, 1, w, h, &draws);  // v3.47 DF347: far, then near
                        if (!DofStDisarm(dev)) ok = 0;
                    }
                } else if (!ds) {
                    static int s_logged;
                    if (!s_logged++) Log("depth of field: the Options frame has no depth-stencil surface: the preview shows no depth of field");
                }
            }
            DofCtxRestoreP(dev, &g_dofPv);
        }
        if (!ok) {
            static volatile LONG s_q;
            if (LogQuota(&s_q, 3)) Log("depth of field: the Screen Effects preview could not draw the depth of field (the page keeps working)");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("depth of field", "the Screen Effects preview faulted (logged once)");
        DofCtxRescue(dev, &g_dofPv);  // DF343-001/001b: the saved target, the stencil cleanup, the states
    }
    BgPop();
}

// ---------------------------------------------------------------- install (Init, after ShadowsInstall, before the Options tables)
// the target of the E8 call at va (0 = not a call)
static DWORD DofCallTarget(DWORD va)
{
    const BYTE *p = (const BYTE *)(ULONG_PTR)va;
    return p[0] == 0xE8 ? va + 5 + *(const DWORD *)(p + 1) : 0;
}
static void DofInstall(void)
{
    static const Site kGrid = { 0x4EFBCA, { 0xE8, 0xB1, 0xE1, 0x06, 0x00, 0x85, 0xC0, 0x74, 0x0E }, 9 };  // call 55DD80; test eax,eax; je
    BYTE oldReset[5];
    char buf[32];
    int v;
    DWORD next;
    IniStr("ScreenEffects", "DepthOfField", buf, sizeof buf);
    v = DofParseMode(buf);
    if (v < 0) {
        if (buf[0]) Log("depth of field: [ScreenEffects] DepthOfField=%s is not a value (Off, Cut-scenes, Always): Off", buf);
        v = DOF_OFF;
    }
    g_dofMode = v;
    IniStr("ScreenEffects", "DofStrength", buf, sizeof buf);
    {   // v3.45 QS345: Low / Medium / High; a v3.43 / v3.44 value (no DofStrengthV marker) is migrated once
        int rewrite = 0, mark = 0;
        // v3.45 QS345-02: under the ini fallback (no file could be made; the values may be imported from an older ini) a
        // value reads with the legacy meaning and nothing is written
        const int marker = g_iniFallback ? 0 : GetPrivateProfileIntA("ScreenEffects", "DofStrengthV", 0, g_iniPath);
        v = DofStrMigrate(buf, marker, &rewrite, &mark);
        g_dofStrMarked = marker >= DOF_STR_VER;
        if (g_iniFallback) rewrite = mark = 0;
        if (v < 0) {
            if (buf[0]) Log("depth of field: [ScreenEffects] DofStrength=%s is not a value (Low, Medium, High): Medium", buf);
            v = DOFS_MEDIUM;
        }
        if (rewrite) {
            const BOOL ok = WritePrivateProfileStringA("ScreenEffects", "DofStrength", kDofStrName[v], g_iniPath);
            if (!ok) mark = 0;  // not marked: the next start tries again (a "Medium" written reads as Medium either way)
            Log("depth of field: [ScreenEffects] DofStrength=%s (v3.43 / v3.44) is v3.45's %s (the same look; v3.45 has a new, gentler "
                "Low): %s%s%s", buf, kDofStrName[v], ok ? "written as " : "could not be written to SH4Refined.ini (used for this start)",
                ok ? kDofStrName[v] : "", ok ? " (marker DofStrengthV=" DOF_STR_VER_S ")" : "");
        }
        if (mark) {
            const BOOL ok = WritePrivateProfileStringA("ScreenEffects", "DofStrengthV", DOF_STR_VER_S, g_iniPath);
            if (ok) g_dofStrMarked = 1;  // v3.45 QS345-03
            if (!ok || !rewrite)
                Log("depth of field: [ScreenEffects] DofStrength=%s read as %s; the marker DofStrengthV=" DOF_STR_VER_S " %s", buf,
                    kDofStrName[v], ok ? "was written (once: a later Low stays Low)" : "could not be written");
        }
    }
    g_dofStr = v;
    g_dofPrevOn = GetPrivateProfileIntA("ScreenEffects", "DofPreview", 1, g_iniPath) != 0;
    g_dofProbeOn = GetPrivateProfileIntA("ScreenEffects", "DofProbe", 1, g_iniPath) != 0;
    g_dofLogN = GetPrivateProfileIntA("ScreenEffects", "DofLog", 0, g_iniPath) ? 120 : 0;
    g_dofProbeState = g_dofProbeOn ? 0 : 1;
    if (!g_fxOn) {
        Log("depth of field: not available (screen effects not installed: their device helpers); [ScreenEffects] DepthOfField=%s not used",
            kDofModeName[(int)g_dofMode]);
        return;
    }
    next = DofCallTarget(0x402E0D);
    if (!SiteOk(&kGrid) || !next) {
        Log("depth of field: %s is not stock (another mod?): no depth of field ([ScreenEffects] DepthOfField=%s not used)",
            !SiteOk(&kGrid) ? "the render frame's light count call 004EFBCA" : "the device reset call 00402E0D", kDofModeName[(int)g_dofMode]);
        return;
    }
    memcpy(oldReset, (const void *)0x402E0D, 5);
    g_dofResetNext = next;
    if (!PokeBranch(0x402E0D, 0xE8, (const void *)hkDofBeforeReset, 5)) {
        Log("depth of field: 00402E0D could not be written: no depth of field");
        return;
    }
    if (!PokeBranch(0x4EFBCA, 0xE8, (const void *)hkDofGrid, 5)) {
        Poke(0x402E0D, oldReset, 5);  // all or nothing
        Log("depth of field: 004EFBCA could not be written: no depth of field (00402E0D put back%s)",
            memcmp((const void *)0x402E0D, oldReset, 5) ? ": FAILED" : "");
        return;
    }
    g_dofOn = 1;
    if ((ULONG_PTR)g_dofM & 15u) LogAnomaly("depth of field", "the matrix buffer %p is not 16-byte aligned: 0x55BF50 will fault", (void *)g_dofM);
    Log("depth of field: hooks in place (the render frame before the light grid 004EFBCA; the device reset 00402E0D chained to %08lX); "
        "DepthOfField %s, Strength %s; probe %s; preview %s", g_dofResetNext, kDofModeName[(int)g_dofMode], kDofStrName[(int)g_dofStr],
        g_dofProbeOn ? "on (once per room)" : "off ([ScreenEffects] DofProbe=0)",
        !g_dofPrevOn ? "off ([ScreenEffects] DofPreview=0)" : g_fxPrevHook ? "on (the Screen Effects page, 0x526A53)" : "off (no page backdrop hook)");
    LogV("depth of field: the blur eases in: Low %d far + %d near slices, Medium %d + %d, High %d + %d (a quarter level each, the "
         "ramps centred on v3.46's steps)", DofRampN(DOFS_LOW, DOF_FAR), DofRampN(DOFS_LOW, DOF_NEAR), DofRampN(DOFS_MEDIUM, DOF_FAR),
         DofRampN(DOFS_MEDIUM, DOF_NEAR), DofRampN(DOFS_HIGH, DOF_FAR), DofRampN(DOFS_HIGH, DOF_NEAR));  // v3.47 DF347
}
