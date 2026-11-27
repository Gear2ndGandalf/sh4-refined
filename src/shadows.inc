// ================================================================ v3.40 SH340: Shadow Edge (softer shadows) and its preview
// Request: USER_FEEDBACK_v339 "Softer shadow option with preview"; the user's answers: Stock / Soft / Softer (Soft about
// 3 px at 720 lines, Softer about 6 px, scaled with the resolution, the inside as dark as stock); the preview a split view
// of the paused scene (Stock left of a line, the chosen edge right), from the title a figure's shadow on the preview
// picture. Plan: staging/v340_feedback/w1_shadows/PLAN.md; research RESEARCH_shadows.md there. Pure logic: shadows_sim.h
// (test\shadows_sim_test.c). Included by sh4windowmod.c after screenfx.inc (its device helpers: FxSave / FxBase /
// FxRestore, FxVtx4, the ps.1.1 token macros, FxDrawPic, FxPrevLoad, FxPanel).
//
// The site (byte-checked with its pushes, all or nothing with the reset site, listed in SH4WM_Patches):
//   0x55FE43 `call dword [ecx+118h]` = DrawPrimitive(dev, TRIANGLELIST, 0, 4) in ShadowCompositeDraw 0x55F660: the mask
//   (0x12E7C40, stage 0) laid twice over the back buffer. -> `call ShDrawHook; nop` (stdcall, 4 arguments, as the
//   callee-pop COM call). Stock edge and no preview capture: the call is forwarded unchanged.
//   0x402E0D `call 401000` (a bare `ret`) at the top of the device reset 0x402CD0's release sequence (before 0x402C70,
//   0x417680 and Reset): our DEFAULT-pool render targets are released there (D3D8 Reset fails while any exists); they
//   are made again, lazily, by the next composite. Neither site is a Playable Eileen hook (its shadow hook is 0x55FC56,
//   before the mask pass) nor in a cheat-table span (ct_conflicts --span 0x55FE3C-0x55FE49 0x402E0C-0x402E12: none).
// Soft: the chain of the plan (v3.41: down / blur / up into targets without a depth buffer), drawn back by a ps.1.1
// shader: T = the blurred coverage, A = K (T + T (1 - T)), K = 1 - (1 - a)^2, colour = TEXTUREFACTOR (the frame's
// darkness: the stage's, a cut-scene's, Playable Eileen's ShadowDarkness included; tinted stages 9-11 keep their colour),
// SRCALPHA / INVSRCALPHA: the inside is the stock two copies exactly (shadows_sim_test). Every device
// state we change is read first and put back; any failure puts the stock draw back for the session (logged). The
// composite also runs inline after some event-scene models (0x4DE412 ...): each call is handled on its own.
// v3.41 SH341 (USER_FEEDBACK_v340 item 2): one soft edge, Stock / Soft; Soft = 12 px at 720 lines (twice v3.40's Softer),
// built so that no draw stretches more than 2x: down in 2x steps (the first one turns the mask's alpha into coverage),
// Kawase blur passes at the small level, up in 2x steps, the composite from the half size with a core curve (the inside
// exactly stock's darkness). The draws come from shadows_sim.h's plan (ShMakePlan / ShBuildSteps); this file runs them.
// v3.41 SH341 (the pause hitch, the user's log 2026-11-22 09:27): every pause made two full-size targets for the preview
// and freed them at the resume, and the Screen Effects page's pause picture was read back from the GPU (FxGrabClean: a
// lockable copy + LockRect = a GPU sync) in the same frame: 22-36 ms in 7 frames (v3.39 already had 19.7 ms of it, without
// any shadow copy). Now the preview's targets are made at the first pause and kept for the session (released before a
// reset, forgotten on a new device, freed at a pause's end when the address space runs low), and the pause picture is a
// GPU copy into a kept target of ours (ShPauseGrab / ShPauseTex for screenfx.inc): nothing is read back at a pause.
// The preview: in the frame whose picture becomes the pause picture ([0x12E3E2C] == 1) the first composite keeps a copy
// of the frame so far (no shadow yet) and of the mask; on the Shadows page (Option 3 > Realtime Shadow, Enter; hidden
// logical page OP_SHD) that frame is the backdrop (0x526A53, screenfx.inc FxBackdropC) and the mask is laid over it twice:
// the stock way left of a split line, through the chosen edge right, live as the row changes. Opened from the title (or
// with no copy of this pause): a figure's shadow (hard-edged triangles drawn into a mask of our own) on preview.dds / the
// hallway, split the same way, with the last darkness seen. The copies end when play resumes or the game leaves play
// (v3.41: their targets stay for the next pause), and are released when the device resets.
// v3.42 SM342 (USER_FEEDBACK_v341: "Soft shadow looks good now, but it's pitch black"; the user's choice): a Darkness row
// under Shadow Edge, 100 / 85 / 70 / 55 % (v3.43 SH343: + 40 / 25 %) ([Shadows] Darkness, default 85, any 0..100 from the ini): the share of the
// frame's inside darkness kept, applied after Playable Eileen's ShadowDarkness (its TEXTUREFACTOR write at 0x55FC56,
// before the mask pass, is not touched: the hook reads the live value), the colour kept (tinted stages 9-11). Soft: the
// composite's K x d. Stock: the game's own draw, with stage 0's ALPHAARG1 = TFACTOR and an alpha byte from
// shadows_sim.h ShStockDarkByte for that draw only (both put back at once); 100 % forwards the call untouched. The split
// preview: left of the line the game's own shadow (Stock, 100 %), right the chosen edge at the chosen darkness, live.
// v3.44 CS344 (USER_FEEDBACK_v343 item 1: "Changing shadows softness [Darkness] does not work in preview or game"): the
// composite shader is shadows_sim.h's kShCompPs, rebuilt without the `1-x` source modifier that GOG's translator dropped
// silently (the draw then ran without a shader: the coverage, black, at any Darkness); a pixel check (post.inc PxCheck:
// a fixed test draw through the composite and the real c1 into a tiny target of ours, read back) runs at each new
// (darkness, Darkness) on the page and in play, so the log proves the pixels.
// v3.52 SH352 (USER_FEEDBACK_v351 D5, D8): the Darkness row runs 25 -> 100 % (Left lighter, Right darker; the ini's
// percentage unchanged). D8: room 4:1F's "0 soft" was the game's own darkness 0 there (0x55F660 sets TEXTUREFACTOR 0
// for stage 4 room 0x1F): an empty mask, no shadow on screen, nothing to soften. Every composite now carries a reason
// (shadows_sim.h SHN_*): the room line ends with the hard count and its reasons and the count with no darkness, the
// first composite of each reason in a room is logged once (bounded), and the Stock / off path is counted too.
#include "shadows_sim.h"

typedef HRESULT(STDMETHODCALLTYPE *ShDP_t)(void *, DWORD, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE *ShSetRT_t)(void *, void *, void *);
typedef HRESULT(STDMETHODCALLTYPE *ShTexDesc_t)(void *, UINT, FxSurfDesc8 *);
typedef HRESULT(STDMETHODCALLTYPE *ShTexSurf_t)(void *, UINT, void **);
enum { SHV_COPYRECTS = 28, SHV_SETRT = 31, SHV_GETRT = 32, SHV_GETDS = 33, SHV_DP = 70 };

// ---------------------------------------------------------------- settings and state
static volatile LONG g_shEdge;          // [Shadows] Edge: 0 Stock, 1 Soft (v3.41 SH341: v3.40's Softer reads as Soft)
static LONG g_snapShEdge;               // as Options opened
static volatile LONG g_shDark = SH_DARK_DEF;  // v3.42 SM342: [Shadows] Darkness, percent 0..100 (the row: 100 / 85 / 70 / 55; v3.43 SH343: + 40 / 25; v3.52 SH352: 25 .. 100, left to right)
static LONG g_snapShDark;               // v3.42 SM342: as Options opened
static float g_shSoftPx = SH_SOFT_PX720;  // hidden [Shadows] SoftPx (at 720 lines; v3.41 SH341: 12, SofterPx is no longer read)
static int g_shPrevOn = 1;              // hidden [Shadows] Preview (0: the page has no preview, nothing is kept at a pause)
static int g_shLogN;                    // hidden [Shadows] Log=1: the next 120 composites logged in detail
static int g_shOn;                      // both sites are ours
static int g_shMenu;                    // Option 3's Realtime Shadow row opens the page
static int g_shFailed;                  // the session's stock fallback (kShFail index)
static DWORD g_shStockIn = 0x524F20;    // the row's stock input (kept from the copied row)
static void *g_shDev;
static DWORD g_shPsDown, g_shPsComp;    // our pixel shaders (0 = not made): the 4-tap mean (down / blur / up), the composite
static DWORD g_shPsNorm;                // v3.41 SH341: the first down pass (the mask's alpha -> coverage 0..1)
static int g_shDsMode;                  // our targets' depth: 0 not tried yet, 1 none, 2 the game's
static DWORD g_shLastTf;                // the last darkness a composite drew with (the card's)
static int g_shRtFails;                 // render targets that could not be made in a row (3: off for the session)
typedef struct { void *tex, *surf; int w, h; DWORD fmt; } ShRt;
static ShRt g_shChain[SHB_N];               // v3.41 SH341: [SHB_L1 .. SHB_L4] the levels, [SHB_X] the blur's second target
static ShRt g_shClean, g_shMaskRt;          // the preview: the frame without the shadow, the mask (or the card's)
static ShRt g_shPause;                      // v3.41 SH341: the Screen Effects page's pause picture (a GPU copy, no read back)
static LONG g_shPauseSeq = -1;              // the pause picture's g_fxPausePicSeq (screenfx.inc) when it was copied
static int g_shPauseFails, g_shWasPaused, g_shKeptLogged;
static unsigned g_shVram;                   // bytes of our render targets now
static ShPlan g_shPlan;                     // the plan in use (edge, size)
static int g_shPlanOk;
// the preview's copies: taken in the frame g_shCapFrame (g_fxFrame); the latest pause picture's frame g_shPauseFrame
static int g_shCopies, g_shCardOn, g_shCardW, g_shCardH;
static LONG g_shCapFrame = -2, g_shPauseFrame = -3;
static DWORD g_shCapTf, g_shCardTf;
static int g_shPageWas, g_shModeWas = -1, g_shWhyWas = -1;
static volatile LONG g_shInHook;
// per room (the room line) and per frame
static DWORD g_shRoomKey = 0xFFFFFFFFu;
static unsigned g_shRoomCalls, g_shRoomSoft, g_shRoomFrames, g_shRoomMaxN;
static double g_shRoomMs;
static float g_shRoomMsMax;
static LONG g_shFrameSeen = -1;
static unsigned g_shFrameN;
static LARGE_INTEGER g_shQf;

// ---------------------------------------------------------------- texts and the page
enum { SHM_T, SHM_EDGE_L, SHM_BACK_L, SHM_EDGE_D, SHM_EDGE_DP, SHM_EDGE_NA, SHM_BACK_D, SHM_ROW_D, SHM_V0,  // v3.41 SH341: v3.40's 3 value slots kept (ids never move)
       SHM_DARK_L = SHM_V0 + SH_EDGE_IDS, SHM_DARK_D, SHM_DARK_DP, SHM_DV0,  // v3.42 SM342: the Darkness row, appended
       SHM_N = SHM_DV0 + SH_DARK_N };  // v3.43 SH343: 18 -> 20 of the SHD block's 32 (40 / 25 % take the next two ids)
static BYTE g_txtSh[SHM_N][64];
static DWORD g_shIds[SH_EDGE_N];
static DWORD g_shDarkIds[SH_DARK_N];  // v3.42 SM342
static DWORD g_rowsShd[3 * 7 + 1];  // Shadow Edge, Darkness (v3.42 SM342), Back
typedef char ShTextsFit[SHM_N <= MSG_DOF0 - MSG_SHD0 ? 1 : -1];  // v3.43 MERGE: ends at the next block (was MSG_END)
static const BYTE *ShText(int id)
{
    return id >= MSG_SHD0 && id < MSG_SHD0 + SHM_N ? g_txtSh[id - MSG_SHD0] : g_txtSh[SHM_EDGE_NA];
}
static void ShTexts(void)
{
    int v;
    // v3.53 OL1: the plan's words (Shadow Edge -> Edge, Shadow Darkness -> Darkness; the row's Enter names the page's rows)
    Encode(g_txtSh[SHM_T], 64, OLW_T_SHADOWS);
    Encode(g_txtSh[SHM_EDGE_L], 64, OLW_SHADOWS_EDGE_L);
    Encode(g_txtSh[SHM_BACK_L], 64, OLW_BACK);
    Encode(g_txtSh[SHM_EDGE_D], 64, OLW_SHADOWS_EDGE_D);  // v3.42 SM342 (was "...: Stock.")
    Encode(g_txtSh[SHM_EDGE_DP], 64, OLW_SHD_EDGE_NOPREVIEW);  // v3.41 SH341 (was "Stock, Soft or Softer.")
    Encode(g_txtSh[SHM_EDGE_NA], 64, OLW_SHD_EDGE_FAILED);
    Encode(g_txtSh[SHM_BACK_D], 64, OLW_BACK_INTERFACE);
    Encode(g_txtSh[SHM_ROW_D], 64, OLW_COMBO_SHADOWS);
    for (v = 0; v < SH_EDGE_N; ++v) {
        Encode(g_txtSh[SHM_V0 + v], 64, kShEdgeName[v]);
        g_shIds[v] = MSG_SHD0 + SHM_V0 + v;
    }
    Encode(g_txtSh[SHM_V0 + 2], 64, kShEdgeName[SH_SOFT]);  // v3.41 SH341: v3.40's Softer slot (not in the row) reads Soft
    // v3.42 SM342: the Darkness row (label, two descriptions, the four values)
    Encode(g_txtSh[SHM_DARK_L], 64, OLW_SHADOWS_DARKNESS_L);  // v3.53 OL1
    Encode(g_txtSh[SHM_DARK_D], 64, OLW_SHADOWS_DARKNESS_D);
    Encode(g_txtSh[SHM_DARK_DP], 64, OLW_SHD_DARK_NOPREVIEW);
    for (v = 0; v < SH_DARK_N; ++v) {
        char t[8];
        sprintf_s(t, sizeof t, "%d %%", kShDarkPct[v]);
        Encode(g_txtSh[SHM_DV0 + v], 64, t);
        g_shDarkIds[v] = MSG_SHD0 + SHM_DV0 + v;
    }
}
// v3.42 SM342: Darkness (percent; the row shows the nearest of its four values)
static float ShDarkNow(void) { return ShDarkD((int)g_shDark); }
static void ShDarkSet(int pct, int save)
{
    char t[16];
    pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
    InterlockedExchange(&g_shDark, pct);
    sprintf_s(t, sizeof t, "%d", pct);
    if (save && !g_iniFallback) WritePrivateProfileStringA("Shadows", "Darkness", t, g_iniPath);
    LogV("shadows: Darkness %d %% (of the game's inside darkness, after Playable Eileen's ShadowDarkness; Stock and Soft)", pct);
}
static int __cdecl ShDarkGet(void) { return ShDarkIndex((int)g_shDark); }
static int __cdecl ShDarkIn(BYTE *cfg, int count)
{
    const int v = StepValue(ShDarkGet(), count);
    (void)cfg;
    if (v >= 0 && v < SH_DARK_N) ShDarkSet(kShDarkPct[v], 1);
    return 0;
}
static int __cdecl ShDarkDesc(void) { return MSG_SHD0 + (g_shPrevOn && g_fxPrevHook && !PvPreviewOff() ? SHM_DARK_D : SHM_DARK_DP); }  // v3.58 PV358: no "line" without the preview
static void ShSet(int v, int save)
{
    if (v < 0 || v >= SH_EDGE_N) return;
    InterlockedExchange(&g_shEdge, v);
    if (save && !g_iniFallback) WritePrivateProfileStringA("Shadows", "Edge", kShEdgeName[v], g_iniPath);
    if (g_shLogN >= 0 && GetPrivateProfileIntA("Shadows", "Log", 0, g_iniPath)) g_shLogN = 120;
    LogV("shadows: Edge %s%s", kShEdgeName[v], g_shFailed ? " (off this session: the game's own edge is drawn)" : "");
}
static int __cdecl ShEdgeIn(BYTE *cfg, int count)
{
    const int v = StepValue((int)g_shEdge, count);
    (void)cfg;
    if (v >= 0) ShSet(v, 1);
    return 0;
}
static int __cdecl ShEdgeGet(void) { return (int)g_shEdge >= 0 && (int)g_shEdge < SH_EDGE_N ? (int)g_shEdge : 0; }
static int __cdecl ShEdgeDesc(void)
{
    if (g_shFailed) return MSG_SHD0 + SHM_EDGE_NA;
    return MSG_V53 + (ShEdgeGet() == SH_SOFT ? OLT_SHE_VD_SOFT : OLT_SHE_VD_STOCK);  // v3.53 OL1: a line per value (the plan's; both have one)
}
static int __cdecl ShBackDesc(void) { return MSG_SHD0 + SHM_BACK_D; }
static int __cdecl ShBackIn(BYTE *cfg, int count)  // Enter fades back to Option 3, on Realtime Shadow
{
    (void)cfg;
    (void)count;
    if (!((Fn2)0x4F84A0)(0, 0x11)) return 0;
    InterlockedExchange(&g_subReq, REQ_BACK);
    ((Fn1)0x56CC40)(0x9C42);  // the stock confirm sound (0x5261CB)
    return SubPageExit();
}
// Option 3 row 6 (Realtime Shadow, stock input 0x524F20: confirm / arrows / Left / Right toggle cfg+0x19): Enter opens the
// Shadows page (the fade route only, as Option 5's folders); Left / Right / the arrows stay stock
static int __cdecl ShRowInput(BYTE *cfg, int count)
{
    if (g_shMenu && g_subScreenOn && ((Fn2)0x4F84A0)(0, 0x11)) {
        InterlockedExchange(&g_subReq, REQ_OPEN | LP_SHADOWS);  // v3.54 OL34: (not on a page any more: Interface keeps the stock input)
        ((Fn1)0x56CC40)(0xA411);  // the stock sub-screen sound (Button Config, 52495B)
        return SubPageExit();
    }
    return ((int(__cdecl *)(BYTE *, int))(ULONG_PTR)g_shStockIn)(cfg, count);
}
static int __cdecl ShRowDesc(void) { return MSG_SHD0 + SHM_ROW_D; }
static int ShRowWanted(void) { return g_shOn; }
static void ShRowInstall(DWORD *row)  // InstallPages: the copied Option 3 table's row 6 (checked stock there)
{
    g_shStockIn = row[0];
    row[0] = (DWORD)(ULONG_PTR)ShRowInput;
    row[6] = (DWORD)(ULONG_PTR)ShRowDesc;
    g_shMenu = 1;
    g_opParent[OP_SHD] = 2;
    g_opParentRow[OP_SHD] = 6;
    LogV("shadows: Option 3's Realtime Shadow row (row 6) opens the Shadows page (Enter; Left / Right stay On / Off)");
}
static int ShMenuOn(void) { return g_shMenu; }
static void ShMenuOff(const char *why)
{
    if (!g_shMenu) return;
    g_shMenu = 0;
    g_opParent[OP_SHD] = -1;
    Log("shadows: no Shadows page (%s); Enter on Realtime Shadow is stock; [Shadows] Edge in SH4Refined.ini still applies", why);
}
// ApplyAspectMenu: the page's table (Shadow Edge, Back), from Option 1's stock rows (Exit at row 6)
static void ShBuildPage(const DWORD *stockRows)
{
    if (!g_shOn) return;
    ShTexts();
    FillRow(g_rowsShd, ShEdgeIn, ShEdgeGet, g_shIds, SH_EDGE_N, MSG_SHD0 + SHM_EDGE_L, ShEdgeDesc);
    FillRow(g_rowsShd + 7, ShDarkIn, ShDarkGet, g_shDarkIds, SH_DARK_N, MSG_SHD0 + SHM_DARK_L, ShDarkDesc);  // v3.42 SM342
    memcpy(g_rowsShd + 14, stockRows + 6 * 7, 7 * 4);  // v3.42 SM342: Back is row 2 (was row 1)
    g_rowsShd[14 + 0] = (DWORD)(ULONG_PTR)ShBackIn;
    g_rowsShd[14 + 5] = MSG_SHD0 + SHM_BACK_L;
    g_rowsShd[14 + 6] = (DWORD)(ULONG_PTR)ShBackDesc;
    g_rowsShd[21] = 0;
    g_opTable[OP_SHD] = g_rowsShd;
    g_opCount[OP_SHD] = 3;  // v3.42 SM342: + Darkness
    g_opTitle[OP_SHD] = MSG_SHD0 + SHM_T;
}
static int ShPageShown(void) { return g_shMenu && (g_lpage == LP_SHADOWS || g_pending == LP_SHADOWS); }  // v3.54 OL34: Effects > Shadows
static void ShSnapshot(void) { g_snapShEdge = g_shEdge, g_snapShDark = g_shDark; }  // v3.42 SM342: + Darkness
static int ShChanged(void) { return g_snapOn && g_shMenu && (g_shEdge != g_snapShEdge || g_shDark != g_snapShDark); }
static int ShRowChanged(int row)  // v3.42 SM342: row 1 = Darkness
{
    if (!g_snapOn || !g_shMenu) return 0;
    return row == 0 ? g_shEdge != g_snapShEdge : row == 1 ? g_shDark != g_snapShDark : 0;
}
static void ShDiscard(void)
{
    if (g_snapOn && g_shEdge != g_snapShEdge) ShSet((int)g_snapShEdge, 1);
    if (g_snapOn && g_shDark != g_snapShDark) ShDarkSet((int)g_snapShDark, 1);  // v3.42 SM342
}

// ---------------------------------------------------------------- render targets (DEFAULT pool, game thread)
static float ShMb(unsigned b) { return (float)b / (1024.0f * 1024.0f); }
static void ShRtFree(ShRt *r, const char *what, int log)
{
    const unsigned b = (unsigned)r->w * (unsigned)r->h * 4u;
    if (r->surf) ((Release_t)VT(r->surf, VT_RELEASE))(r->surf);
    if (r->tex) ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
    if (r->tex) {
        g_shVram = g_shVram >= b ? g_shVram - b : 0;
        if (log) LogV("shadows: render target released: %s %dx%d (ours now %.1f MB)", what, r->w, r->h, ShMb(g_shVram));
    }
    memset(r, 0, sizeof *r);
}
static int ShRtMake(void *dev, ShRt *r, int w, int h, DWORD fmt, const char *what)
{
    HRESULT hr;
    if (r->tex && r->w == w && r->h == h && r->fmt == fmt) return 1;
    ShRtFree(r, what, 1);
    hr = ((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, (UINT)w, (UINT)h, 1, 1 /*RENDERTARGET*/, fmt, 0 /*DEFAULT*/, &r->tex);
    if (FAILED(hr) || !r->tex) {
        r->tex = NULL;
        Log("shadows: a %dx%d render target (%s, format %lu) could not be made (hr %08lX)", w, h, what, fmt, (unsigned long)hr);
        return 0;
    }
    hr = ((ShTexSurf_t)VT(r->tex, 15))(r->tex, 0, &r->surf);
    if (FAILED(hr) || !r->surf) {
        r->surf = NULL;
        ((Release_t)VT(r->tex, VT_RELEASE))(r->tex);
        r->tex = NULL;
        Log("shadows: the %s render target's surface could not be read (hr %08lX)", what, (unsigned long)hr);
        return 0;
    }
    r->w = w, r->h = h, r->fmt = fmt;
    g_shVram += (unsigned)w * (unsigned)h * 4u;
    LogV("shadows: render target made: %s %dx%d (%.1f MB; ours now %.1f MB)", what, w, h, ShMb((unsigned)w * (unsigned)h * 4u), ShMb(g_shVram));
    return 1;
}
static const char *const kShChainName[SHB_N] = { "", "half size", "quarter size", "eighth size", "sixteenth size",
                                                 "the blur's second target" };  // v3.41 SH341: by buffer (SHB_*)
// v3.41 SH341: the free address space (MB) of this 32-bit process: the kept full-size targets go when it runs low
static int ShVaMarginMb(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    return GlobalMemoryStatusEx(&ms) ? (int)(ms.ullAvailVirtual >> 20) : 0;
}
static void ShFreeCopies(const char *why, int withPause)  // v3.41 SH341: + withPause (the Screen Effects page's picture too)
{
    if (!g_shCopies && !g_shCardOn && !g_shClean.tex && !g_shMaskRt.tex && !(withPause && g_shPause.tex)) return;
    ShRtFree(&g_shClean, "the paused scene", 0);
    ShRtFree(&g_shMaskRt, g_shCardOn ? "the figure's mask" : "the paused mask", 0);
    if (withPause) ShRtFree(&g_shPause, "the pause picture", 0), g_shPauseSeq = -1;
    LogV("shadows: the preview's copies freed (%s; ours now %.1f MB)", why, ShMb(g_shVram));
    g_shCopies = g_shCardOn = 0;
    g_shCapFrame = -2;
}
// v3.41 SH341: the copies' contents end (play resumed, the page closed, ...): their targets stay for the next pause
static void ShDropCopies(void)
{
    g_shCopies = g_shCardOn = 0;
    g_shCapFrame = -2;
}
// v3.41 SH341: a gameplay pause ended: the kept full-size targets stay unless the address space runs low
static void ShPauseEnded(void)
{
    const unsigned full = ShPicBytes(g_shClean.w, g_shClean.h) + ShPicBytes(g_shMaskRt.w, g_shMaskRt.h) + ShPicBytes(g_shPause.w, g_shPause.h);
    int margin;
    if (!g_shClean.tex && !g_shMaskRt.tex && !g_shPause.tex) return;
    margin = ShVaMarginMb();
    if (!ShKeepTargets(margin)) {
        char why[96];
        sprintf_s(why, sizeof why, "the address space has %d MB left, under %d: made again at the next pause", margin, SH_KEEP_MARGIN_MB);
        ShFreeCopies(why, 1);
        g_shKeptLogged = 0;
        return;
    }
    if (!g_shKeptLogged++)
        LogV("shadows: the pause ended: its full-size targets (%.1f MB: the preview's copies and the pause picture) are kept for the next "
             "pause (the address space has %d MB left); ours now %.1f MB", ShMb(full), margin, ShMb(g_shVram));
}
static void ShFreeAll(const char *why)
{
    const unsigned had = g_shVram;
    int k, n = 0;
    for (k = SHB_L1; k < SHB_N; ++k)
        if (g_shChain[k].tex) ++n, ShRtFree(&g_shChain[k], kShChainName[k], 0);
    n += (g_shClean.tex != NULL) + (g_shMaskRt.tex != NULL) + (g_shPause.tex != NULL);
    ShRtFree(&g_shClean, "the paused scene", 0);
    ShRtFree(&g_shMaskRt, "the mask copy", 0);
    ShRtFree(&g_shPause, "the pause picture", 0);  // v3.41 SH341
    g_shPauseSeq = -1;
    g_shCopies = g_shCardOn = 0;
    g_shCapFrame = -2;
    g_shPlanOk = 0;
    g_shKeptLogged = 0;
    if (n) LogV("shadows: %s: %d render target(s) released (%.1f MB)", why, n, ShMb(had));
}
// v3.41 REV341 SH341-R1: our targets made on an older device are released too (each holds a reference on that device:
// zeroing the pointers leaked them and the device). Guarded: if the old objects are already gone, only forgotten.
static void ShFreeOld(const char *why)
{
    __try {
        ShFreeAll(why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("shadows", "releasing the render targets of an older Direct3D device faulted: forgotten instead (logged once)");
        memset(g_shChain, 0, sizeof g_shChain), memset(&g_shClean, 0, sizeof g_shClean), memset(&g_shMaskRt, 0, sizeof g_shMaskRt);
        memset(&g_shPause, 0, sizeof g_shPause), g_shPauseSeq = -1, g_shVram = 0, g_shCopies = g_shCardOn = 0, g_shPlanOk = 0;
    }
}
// 0x402E0D (was `call 401000`, a bare ret): the device reset 0x402CD0 is about to release its own DEFAULT objects and Reset
static void __cdecl ShBeforeReset(void)
{
    __try {
        if (g_shDev == DEV) ShFreeAll("device reset");
        else ShFreeOld("device reset (targets of an older device)");  // v3.41 REV341 SH341-R1 (was: forgotten, not released)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("shadows", "releasing our render targets before a device reset faulted (logged once)");
    }
    PvBeforeReset();  // v3.56 PV356: the Effects preview's look layers (DEFAULT pool, as ours; guarded inside: preview.inc)
}
static __declspec(naked) void hkShBeforeReset(void)
{
    __asm {
        pushad
        pushfd
        call ShBeforeReset
        popfd
        popad
        ret
    }
}
// a new device: the old device's targets are released first (v3.41 REV341 SH341-R1), then made again on the new one
static void ShDevCheck(void *dev)
{
    if (dev == g_shDev) return;
    if (g_shDev) {
        LogV("shadows: a new Direct3D device (%p): our objects are made again", dev);
        ShFreeOld("a new Direct3D device");
    }
    g_shDev = dev;
    memset(g_shChain, 0, sizeof g_shChain);
    memset(&g_shClean, 0, sizeof g_shClean);
    memset(&g_shMaskRt, 0, sizeof g_shMaskRt);
    memset(&g_shPause, 0, sizeof g_shPause);  // v3.41 SH341
    g_shPauseSeq = -1;
    g_shVram = 0;
    g_shPsDown = g_shPsComp = g_shPsNorm = 0;
    g_shCopies = g_shCardOn = 0;
    g_shPlanOk = 0;
    g_shDsMode = 0;
}

// ---------------------------------------------------------------- shaders (ps.1.1, one constant read per instruction)
// DOWN: the mean of 4 bilinear taps (the 4 x 4 box): c0 = 0.5
static const DWORD kPsShDown[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),         // r0 = (t0 + t1) / 2
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),         // r1 = (t2 + t3) / 2
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),         // r0 = the mean
    PS_END };
// v3.41 SH341 NORM (the first down pass): the mean, x 4 c (c1 = (0, 0, 0, c), c = 1.01 / (32 a)), then x 2 three times
// saturated: coverage sat(32 c x) (1 inside), rgb 0. Proven forms only: mul _x4 r, r, c (the CRT tone) and add_sat r, r, r
static const DWORD kPsShNorm[] = { PS_VER, PS_TEX(0), PS_TEX(1), PS_TEX(2), PS_TEX(3),
    I_LRP, D_R(0), S_C(0), S_T(0), S_T(1),
    I_LRP, D_R(1), S_C(0), S_T(2), S_T(3),
    I_LRP, D_R(0), S_C(0), S_R(0), S_R(1),         // r0 = the mean (alpha = a x the 4 x 4 coverage)
    I_MUL, D_R(0) | D_X4, S_R(0), S_C(1),          // r0 = (0, 0, 0, 4 c x)
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2
    I_ADD, D_R(0) | D_SAT, S_R(0), S_R(0),         // x 2: coverage
    PS_END };
// COMP: v3.44 CS344: shadows_sim.h's kShCompPs (v3.41's `mad r1, r0, 1-r0, r0` drew as if no shader were set)
#define kPsShComp kShCompPs  // v3.44 CS344
static int ShShaders(void *dev)
{
    HRESULT hr;
    if (g_shPsDown && g_shPsComp && g_shPsNorm) return 1;
    if (!g_shPsNorm) {  // v3.41 SH341
        hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kPsShNorm, &g_shPsNorm);
        if (FAILED(hr) || !g_shPsNorm) {
            g_shPsNorm = 0;
            Log("shadows: CreatePixelShader (coverage) failed (hr %08lX)", (unsigned long)hr);
            return 0;
        }
        Log("shadows: coverage shader made (ps.1.1, handle %lu, device %p)", g_shPsNorm, dev);
    }
    if (!g_shPsDown) {
        hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kPsShDown, &g_shPsDown);
        if (FAILED(hr) || !g_shPsDown) {
            g_shPsDown = 0;
            Log("shadows: CreatePixelShader (down) failed (hr %08lX)", (unsigned long)hr);
            return 0;
        }
        Log("shadows: down shader made (ps.1.1, handle %lu, device %p)", g_shPsDown, dev);
    }
    if (g_shPsComp) return 1;
    hr = ((FxCreatePs_t)VT(dev, FXV_CREATEPS))(dev, kPsShComp, &g_shPsComp);
    if (FAILED(hr) || !g_shPsComp) {
        g_shPsComp = 0;
        Log("shadows: CreatePixelShader (composite) failed (hr %08lX)", (unsigned long)hr);
        return 0;
    }
    Log("shadows: composite shader made (ps.1.1, handle %lu, device %p)", g_shPsComp, dev);
    return 1;
}

// ---------------------------------------------------------------- drawing helpers
// A quad with 4 texture coordinate sets (FVF 0x444): the rectangle [xa, xa + wq] x [ya, ya + hq] (u, v 0..1 over it),
// clipped to x in [cx0, cx1); tap k = base + (-+du, -+dv)
static HRESULT ShQuad4(void *dev, float xa, float ya, float wq, float hq, float cx0, float cx1, float du, float dv)
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
// the same with one coordinate set (FVF 0x144) and a colour
static HRESULT ShQuad1(void *dev, float xa, float ya, float wq, float hq, float cx0, float cx1, DWORD c)
{
    const float x0 = xa > cx0 ? xa : cx0, x1 = xa + wq < cx1 ? xa + wq : cx1;
    SpVtx q[4];
    int m;
    if (!(x1 > x0) || !(wq > 0.0f) || !(hq > 0.0f)) return S_OK;
    for (m = 0; m < 4; ++m) {
        const float x = (m & 1) ? x1 : x0, y = (m & 2) ? ya + hq : ya;
        q[m].x = x, q[m].y = y, q[m].z = 0.0f, q[m].rhw = 1.0f, q[m].c = c;
        q[m].u = (x - xa) / wq, q[m].v = (y - ya) / hq;
    }
    return ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 5 /*TRIANGLESTRIP*/, 2, q, sizeof(SpVtx));
}
static void ShPsc(void *dev, int reg, float a, float b, float c, float d)
{
    float v[4];
    v[0] = a, v[1] = b, v[2] = c, v[3] = d;
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, (DWORD)reg, v, 1);
}
static void ShTex4(void *dev, void *tex)
{
    int s;
    for (s = 0; s < 4; ++s) ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, (DWORD)s, tex);
}
// one of our targets (no depth buffer; the game's if the wrapper refuses none), its whole area as the viewport
static int ShTarget(void *dev, const ShRt *r, void *gameDs)
{
    Viewport8 vp;
    HRESULT hr = E_FAIL;
    if (g_shDsMode != 2) {
        hr = ((ShSetRT_t)VT(dev, SHV_SETRT))(dev, r->surf, NULL);
        if (SUCCEEDED(hr) && !g_shDsMode) {
            g_shDsMode = 1;
            LogV("shadows: our render targets are drawn without a depth buffer");
        }
    }
    if (FAILED(hr) && g_shDsMode != 1 && gameDs) {
        const HRESULT h2 = ((ShSetRT_t)VT(dev, SHV_SETRT))(dev, r->surf, gameDs);
        if (SUCCEEDED(h2) && !g_shDsMode) LogV("shadows: no depth buffer refused (hr %08lX): our render targets use the game's (Z and stencil off)", (unsigned long)hr);
        if (SUCCEEDED(h2)) g_shDsMode = 2;
        hr = h2;
    }
    if (FAILED(hr)) return 0;
    vp.x = vp.y = 0, vp.w = (DWORD)r->w, vp.h = (DWORD)r->h, vp.minZ = 0.0f, vp.maxZ = 1.0f;
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &vp);
    return 1;
}
static int ShTargetBack(void *dev, void *rt, void *ds, const Viewport8 *vp)
{
    const HRESULT hr = ((ShSetRT_t)VT(dev, SHV_SETRT))(dev, rt, ds);
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, (Viewport8 *)vp);
    if (FAILED(hr)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("shadows", "the game's render target could not be set back (hr %08lX)", (unsigned long)hr);
        return 0;
    }
    return 1;
}
// the plan for (edge, size): made again when either changes; logged
static const ShPlan *ShPlanFor(int edge, int w, int h)
{
    if (g_shPlanOk && g_shPlan.edge == edge && g_shPlan.w[0] == w && g_shPlan.h[0] == h) return &g_shPlan;
    if (!ShMakePlan(edge, w, h, g_shSoftPx, &g_shPlan)) {
        g_shPlanOk = 0;
        return NULL;
    }
    g_shPlanOk = 1;
    {   // v3.41 SH341: the plan's line (down / blur / up; every draw at most a 2x stretch)
        const ShPlan *p = &g_shPlan;
        char offs[48];
        int k, o = 0;
        offs[0] = 0;
        for (k = 0; k < p->kawase && o < (int)sizeof offs - 8; ++k) o += sprintf_s(offs + o, sizeof offs - o, "%s%.2f", k ? " " : "", p->off[k]);
        LogV("shadows: %s at %dx%d: down to %dx%d (%d level(s)), %d blur pass(es)%s%s%s, up in 2x steps: a %.1f px edge (asked %.1f%s); "
             "%d draws, at most a %.0fx stretch; %.1f MB of render targets", kShEdgeName[edge], w, h, p->w[p->levels], p->h[p->levels],
             p->levels, p->kawase, p->kawase ? " (offsets " : "", offs, p->kawase ? ")" : "", p->width, p->target,
             p->clamped ? ", the nearest the chain reaches" : "", p->nsteps, ShMaxStretch(p), ShMb(ShChainBytes(p)));
    }
    return &g_shPlan;
}
static int ShChainReady(void *dev, const ShPlan *pl)  // v3.41 SH341: the plan's buffers made, the others freed
{
    const unsigned need = ShPlanBufs(pl);
    int b;
    for (b = SHB_L1; b < SHB_N; ++b)
        if ((need & (1u << b)) && !ShRtMake(dev, &g_shChain[b], ShBufW(pl, b), ShBufH(pl, b), 21 /*A8R8G8B8*/, kShChainName[b])) return 0;
    for (b = SHB_L1; b < SHB_N; ++b)
        if (!(need & (1u << b)) && g_shChain[b].tex) ShRtFree(&g_shChain[b], kShChainName[b], 1);
    return 1;
}
// v3.41 SH341: the plan's draws up to the composite, from src (the mask, or our mask copy; darkness alpha a): the
// composite's source texture, NULL on a failure (*fail its kind). The caller has saved every state (FxSave, its render
// target and depth) and puts them back.
static void *ShRunChain(void *dev, void *src, const ShPlan *pl, float a, void *gameDs, int *fail)
{
    void *cur = src;
    int k;
    for (k = 0; k < pl->nsteps && pl->step[k].kind != SHS_COMP; ++k) {
        const ShStep *s = &pl->step[k];
        if (!ShTarget(dev, &g_shChain[s->dst], gameDs)) {
            *fail = SHF_TARGET;
            return NULL;
        }
        FxBase(dev);
        ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 0xF);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, s->kind == SHS_NORM ? g_shPsNorm : g_shPsDown);
        ShPsc(dev, 0, 0.5f, 0.5f, 0.5f, 0.5f);
        if (s->kind == SHS_NORM) ShPsc(dev, 1, 0.0f, 0.0f, 0.0f, ShNormC(a));
        ShTex4(dev, s->src == SHB_MASK ? src : g_shChain[s->src].tex);
        if (FAILED(ShQuad4(dev, -0.5f, -0.5f, (float)s->dw, (float)s->dh, -1e9f, 1e9f, s->du, s->dv))) {
            *fail = SHF_DRAW;
            return NULL;
        }
        cur = g_shChain[s->dst].tex;
    }
    return cur;
}
// the soft composite over the current target (the back buffer): [cx0, cx1) of it; cwe = the colour write mask to use
// v3.43 SH343: pct (the Darkness) instead of d: c1.a = ShCompC1, the one formula of the in-play and the preview draw;
// got != NULL: c1 is read back from the device after the upload, before the draw (-1 = the query failed), the value
// the draw uses
static int ShCompose(void *dev, const ShPlan *pl, void *low, DWORD tf, float w, float h, float cx0, float cx1, DWORD cwe, int pct, float *got)
{
    const ShStep *s = &pl->step[pl->nsteps - 1];  // v3.41 SH341: the composite (the tent from the half size)
    float c[4];
    ShTfColour(tf, c);
    FxBase(dev);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, cwe);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_shPsComp);
    ShPsc(dev, 0, 0.5f, 0.5f, 0.5f, 0.5f);
    ShPsc(dev, 1, 0.0f, 0.0f, 0.0f, ShCompC1(ShTfAlpha(tf), pct));  // v3.42 SM342: K x d (v3.43 SH343: ShCompC1)
    ShPsc(dev, 2, c[0], c[1], c[2], 0.0f);
    if (got) {  // v3.43 SH343: what the draw below reads as c1
        float v[4];
        *got = SUCCEEDED(((FxPsc_t)VT(dev, FXV_GETPSC))(dev, 1, v, 1)) ? v[3] : -1.0f;
    }
    ShTex4(dev, low);
    return s->kind == SHS_COMP && SUCCEEDED(ShQuad4(dev, SH_COMP_X0, SH_COMP_X0, w, h, cx0, cx1, s->du, s->dv));
}
// the stock look (the game's two quads, its states) of a mask texture over [cx0, cx1) of the current target
static void ShComposeStock(void *dev, void *mask, float w, float h, float cx0, float cx1, unsigned s8)  // v3.42 SM342: + s8 (the copies' alpha x s8 / 255)
{
    const DWORD c = (DWORD)(s8 > 255u ? 255u : s8) << 24 | 0xFFFFFFu;  // v3.42 SM342: DIFFUSE's alpha (the game's quads: 0xFFFFFFFF)
    FxBase(dev);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 3 /*COLORARG2*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 4 /*MODULATE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 6 /*ALPHAARG2*/, 2 /*TEXTURE*/);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, mask);
    ShQuad1(dev, 0.0f, 0.0f, w, h, cx0, cx1, c);
    ShQuad1(dev, -SH_STOCK_OFF, -SH_STOCK_OFF, w, h, cx0, cx1, c);
}
// a 1:1 point-sampled copy of a texture into the current target (the mask copy)
static HRESULT ShCopyQuad(void *dev, void *tex, float w, float h)
{
    FxBase(dev);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 0xF);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 2 /*TEXTURE*/);
    FxSampler(dev, tex, 0, 1);
    return ShQuad1(dev, -0.5f, -0.5f, w, h, -1e9f, 1e9f, 0xFFFFFFFFu);
}

// ---------------------------------------------------------------- the composite hook (0x55FE43)
static void ShFail(int kind, HRESULT hr, const char *where)
{
    if (g_shFailed) return;
    g_shFailed = kind > SHF_NONE && kind < SHF_N ? kind : SHF_FAULT;
    Log("shadows: off for this session: %s (hr %08lX, %s): the game's own shadow edge from now on", kShFail[g_shFailed], (unsigned long)hr, where);
}
// v3.52 SH352 (D8): why = the composite's SHN_ reason (SHN_SOFT = drawn soft), tf = the darkness it saw; the room line
// ends with "; not soft: H hard (...), Z with no darkness ..." when any was not soft, and each reason's first composite
// in a room is logged once (bounded: by design 32 lines a session, the hard fallbacks 32)
static ShNsRoom g_shRoomNs;  // v3.52 SH352
static void ShNsNote(DWORD key, int why, DWORD tf)
{
    static volatile LONG s_qDesign, s_qHard;
    const int design = why == SHN_NODARK || why == SHN_STOCK;
    if (!LogQuota(design ? &s_qDesign : &s_qHard, 32)) return;
    if (design)
        LogV("shadows: room %lX:%02lX: a composite not soft: %s (darkness %08lX, edge %s, Darkness %d %%; once a room)", key >> 16,
             key & 0xFFFF, kShNsWhy[why], tf, kShEdgeName[ShEdgeGet()], (int)g_shDark);
    else
        Log("shadows: room %lX:%02lX: a composite not soft: %s (darkness %08lX, edge %s, Darkness %d %%; once a room)", key >> 16,
            key & 0xFFFF, kShNsWhy[why], tf, kShEdgeName[ShEdgeGet()], (int)g_shDark);
}
// v3.58 GX358: the stock Realtime Shadow switch (cfg+0x19, 0 = Off) and the stock test of its two readers now
// (shadows_sim.h ShStockDraws: Off keeps the real-time shadows only in the game's own rooms)
static unsigned g_shRoomOff, g_shCompSeen;  // composites this room while the switch is Off; composites this session
static int ShRealOn(void) { return *(volatile BYTE *)0x1091AE9 != 0; }
static int ShStockShadowsHere(void)
{
    __try {
        return ShStockDraws(ShRealOn(), 0, *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94, (*(volatile int *)0x10775DC & 1) != 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 1;
    }
}
static void ShRoomTick(int why, DWORD tf, float ms)  // v3.52 SH352: why / tf (was soft)
{
    const DWORD key = (DWORD)*(volatile int *)0xFD5A90 << 16 | ((DWORD)*(volatile int *)0xFD5A94 & 0xFFFF);
    if (why < 0 || why >= SHN_N) why = SHN_FAIL;  // v3.52 SH352
    if (key != g_shRoomKey) {
        if (g_shRoomCalls && g_shRoomKey != 0xFFFFFFFFu) {
            char tail[SH_NS_TEXT_MAX], off[80];
            ShNsText(tail, sizeof tail, &g_shRoomNs);  // v3.52 SH352
            off[0] = 0;  // v3.58 GX358: the composites the game drew with Shadows Off (its own Off list)
            if (g_shRoomOff) sprintf_s(off, sizeof off, "; %u with Shadows Off (the game keeps this room's)", g_shRoomOff);
            LogV("shadows: room %lX:%02lX: %u composites over %u frames (at most %u in a frame), %u soft; ours %.3f ms a composite on "
                 "average, %.3f at most; edge %s; render targets %.1f MB; darkness %d %%%s%s", g_shRoomKey >> 16, g_shRoomKey & 0xFFFF, g_shRoomCalls,
                 g_shRoomFrames, g_shRoomMaxN, g_shRoomSoft, g_shRoomCalls ? g_shRoomMs / g_shRoomCalls : 0.0, g_shRoomMsMax,
                 kShEdgeName[ShEdgeGet()], ShMb(g_shVram), (int)g_shDark, tail, off);  // v3.42 SM342: + darkness  // v3.52 SH352: + the tail  // v3.58 GX358: + off
        }
        g_shRoomKey = key;
        g_shRoomOff = 0;  // v3.58 GX358
        g_shRoomCalls = g_shRoomSoft = g_shRoomFrames = g_shRoomMaxN = 0;
        g_shRoomMs = 0.0;
        g_shRoomMsMax = 0.0f;
        ShNsReset(&g_shRoomNs);  // v3.52 SH352
    }
    if (g_shFrameSeen != g_fxFrame) g_shFrameSeen = g_fxFrame, g_shFrameN = 0, ++g_shRoomFrames;
    if (++g_shFrameN > g_shRoomMaxN) g_shRoomMaxN = g_shFrameN;
    ++g_shRoomCalls;
    if (g_shCompSeen++ && !ShRealOn()) {  // v3.58 GX358: with Shadows Off only the game's own rooms draw (the session's first call aside)
        ++g_shRoomOff;
        if (!ShStockShadowsHere()) {
            static volatile LONG s_q;
            if (LogQuota(&s_q, 4))
                LogAnomaly("shadows", "a real-time shadow composite with Shadows Off in stage %lX room %lX, which is not on the game's Off "
                                      "list (something else drew it; logged 4 times at most)", key >> 16, key & 0xFFFF);
        }
    }
    g_shRoomSoft += why == SHN_SOFT;
    g_shRoomMs += ms;
    if (ms > g_shRoomMsMax) g_shRoomMsMax = ms;
    if (ShNsAdd(&g_shRoomNs, why)) ShNsNote(key, why, tf);  // v3.52 SH352: the reason's first composite in this room
}
// the capture frame's first composite: the frame so far (no shadow) and the mask, for the Shadows page
static float ShMsSince(const LARGE_INTEGER *t0)  // v3.41 SH341
{
    LARGE_INTEGER t1;
    QueryPerformanceCounter(&t1);
    return g_shQf.QuadPart ? (float)((double)(t1.QuadPart - t0->QuadPart) * 1000.0 / (double)g_shQf.QuadPart) : 0.0f;
}
static void ShCapture(void *dev, void *mask, int w, int h, DWORD tf)
{
    void *rt = NULL, *ds = NULL;
    FxSurfDesc8 d;
    FxSaved sv;
    HRESULT hr = E_FAIL;
    int ok = 0, made = 0;
    LARGE_INTEGER t0, t1;
    float msMake = 0.0f, msCopy = 0.0f;
    g_shCapFrame = g_fxFrame;  // once a frame, whatever happens
    g_shCopies = g_shCardOn = 0;  // v3.41 SH341: a figure's mask target is reused (kept), not freed
    if (!g_shQf.QuadPart) QueryPerformanceFrequency(&g_shQf);
    if (FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) || !rt) return;
    QueryPerformanceCounter(&t0);
    if (SUCCEEDED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) && (d.fmt == 21 || d.fmt == 22) && (int)d.w >= w && (int)d.h >= h &&
        ((made = !(g_shClean.tex && g_shClean.w == (int)d.w && g_shClean.h == (int)d.h && g_shClean.fmt == d.fmt &&
                   g_shMaskRt.tex && g_shMaskRt.w == w && g_shMaskRt.h == h)), 1) &&
        ShRtMake(dev, &g_shClean, (int)d.w, (int)d.h, d.fmt, "the paused scene") && ShRtMake(dev, &g_shMaskRt, w, h, 21, "the paused mask")) {
        msMake = ShMsSince(&t0);
        QueryPerformanceCounter(&t1);
        hr = ((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_shClean.surf, NULL);  // MSAA: the wrapper resolves (as 0x4036E0)
        if (SUCCEEDED(hr) && FxSave(dev, &sv, 0)) {
            if (FAILED(((GetSurf_t)VT(dev, SHV_GETDS))(dev, &ds))) ds = NULL;
            if (ShTarget(dev, &g_shMaskRt, ds)) {
                hr = ShCopyQuad(dev, mask, (float)w, (float)h);
                ok = SUCCEEDED(hr);
            }
            ShTargetBack(dev, rt, ds, &sv.vp);
            FxRestore(dev, &sv);
            ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);
            if (ds) ((Release_t)VT(ds, VT_RELEASE))(ds);
        }
        msCopy = ShMsSince(&t1);
    }
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    if (ok) {
        g_shCopies = 1;
        g_shCapTf = tf;
        LogV("shadows: the paused scene kept for the Shadows page: %dx%d picture + mask (%.1f MB; %s %.2f ms, copied %.2f ms), darkness "
             "%08lX (frame %ld)", g_shClean.w, g_shClean.h, ShMb(ShCopyBytes(w, h)), made ? "targets made" : "targets kept from an earlier pause:",
             msMake, msCopy, tf, g_shCapFrame);
    } else {
        static volatile LONG s_q;
        ShFreeCopies("the copy failed", 0);
        if (LogQuota(&s_q, 3)) Log("shadows: the paused scene could not be kept (hr %08lX): the Shadows page shows the sample figure", (unsigned long)hr);
    }
}
// v3.42 SM342: what the Darkness did to a composite, once per (edge, the frame's darkness alpha, Darkness): the proof
// that the inside is the chosen share of the game's (and PE's) darkness
static void ShDarkNote(int edge, DWORD tf, unsigned s8)
{
    static DWORD s_last = 0xFFFFFFFFu;
    static volatile LONG s_q;
    const int a8 = ShTfAlpha(tf), pct = (int)g_shDark;
    const DWORD key = (DWORD)edge << 24 | (DWORD)a8 << 16 | (DWORD)pct;
    const float a = (float)a8 / 255.0f, full = ShStockAlpha(a);
    if (key == s_last || !LogQuota(&s_q, 60)) return;
    s_last = key;
    if (edge == SH_SOFT)
        LogV("shadows: Darkness %d %% on Soft: darkness %08lX, the composite's K %.3f x %.2f: inside %.3f of the game's %.3f", pct,
             tf, ShCompK(a), ShDarkD(pct), ShCompKDark(a, ShDarkD(pct)), full);
    else
        LogV("shadows: Darkness %d %% on Stock: darkness %08lX, the two copies' alpha x %u / 255 (TFACTOR): inside %.3f of the "
             "game's %.3f (asked %.3f)", pct, tf, s8, ShStockDarkInside(a8, s8), full, ShDarkD(pct) * full);
}
// v3.43 SH343: the c1.a a soft composite drew with, read back from the device after the upload and before the draw
// (ShCompose's got; -1 = the query failed): logged once per (path, the frame's darkness alpha, Darkness): play = 1 the
// in-play composite, 0 the Shadows page's right half; the in-play read-back is repeated every SH_C1_EVERY-th composite
// (silent). A read-back that is not the asked value is an anomaly: the draw would not use the Darkness.
static unsigned g_shC1Key[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu }, g_shC1N;
static void ShC1Note(int play, DWORD tf, int pct, float got)
{
    static volatile LONG s_q[2], s_qa;
    const int a8 = ShTfAlpha(tf);
    const unsigned key = ShC1Key(a8, pct);
    const float asked = ShCompC1(a8, pct), game = ShLightThrough(ShStockAlpha((float)a8 / 255.0f));
    const int fresh = key != g_shC1Key[play ? 1 : 0];
    g_shC1Key[play ? 1 : 0] = key;
    if (got >= 0.0f && !ShC1Same(asked, got)) {
        if (LogQuota(&s_qa, 5))
            LogAnomaly("shadows", "%s: c1.a read back %.4f, asked %.4f (darkness %08lX, Darkness %d %%): the draw does not use the Darkness",
                       play ? "the in-play composite" : "the Shadows page's right half", got, asked, tf, pct);
        return;
    }
    if (!fresh || !LogQuota(&s_q[play ? 1 : 0], 40)) return;
    if (got < 0.0f)
        LogV("shadows: %s drew darkness %08lX at Darkness %d %% with c1.a %.3f (not read back: the query failed): the inside lets %.0f %% "
             "of the light through (the game's own %.0f %%)", play ? "in play the composite" : "the Shadows page's right half", tf, pct,
             asked, 100.0f * ShLightThrough(asked), 100.0f * game);
    else if (play)
        LogV("shadows: in play the composite drew darkness %08lX at Darkness %d %%: c1.a %.3f read back from the device before the draw "
             "(asked %.3f; the Shadows page's right half uses the same): the inside lets %.0f %% of the light through (the game's own %.0f %%)",
             tf, pct, got, asked, 100.0f * ShLightThrough(got), 100.0f * game);
    else
        LogV("shadows: the Shadows page's right half drew darkness %08lX at Darkness %d %%: c1.a %.3f read back (asked %.3f): the inside "
             "lets %.0f %% of the light through (left of the line, the game's own: %.0f %%)", tf, pct, got, asked,
             100.0f * ShLightThrough(got), 100.0f * game);
}
// v3.44 CS344: the pixel check of the soft composite (post.inc PxCheck): four cells of coverage kShPxA (rgb 0) drawn
// through the composite shader with c1.a = K (the real draw's) and a black colour over a white target, read back; logged
// once per (path, darkness, Darkness) (the same keys as ShC1Note, before it), rate-limited
typedef struct { float k; } ShPxCtx;
typedef char ShPxCellsFit[PX_CELLS == SH_PX_N ? 1 : -1];
static int ShPxDraw(void *dev, void *src, const void *ctx)
{
    const float k = ((const ShPxCtx *)ctx)->k;
    FxBase(dev);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 7);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x444);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_shPsComp);
    ShPsc(dev, 0, 0.5f, 0.5f, 0.5f, 0.5f);
    ShPsc(dev, 1, 0.0f, 0.0f, 0.0f, k);
    ShPsc(dev, 2, 0.0f, 0.0f, 0.0f, 0.0f);
    ShTex4(dev, src);
    return SUCCEEDED(ShQuad4(dev, -0.5f, -0.5f, (float)PX_W, (float)PX_H, -1e9f, 1e9f, 0.0f, 0.0f));
}
static void ShPxCheck(void *dev, int play, DWORD tf, int pct)
{
    static volatile LONG s_q;
    DWORD src[PX_CELLS];
    PxProbe pr;  // v3.47 PX347 (was out[])
    unsigned char g[SH_PX_N];
    float a[SH_PX_N], off = 0.0f, t[SH_PX_N], tR[SH_PX_N], aR[SH_PX_N], offR = 0.0f;
    const char *why = "", *path = play ? "in play" : "the Shadows page";
    char st[200], who[48];
    ShPxCtx cx;
    int k, v, vR, bad = 0, basis = 0;
    if (!g_pxOn || RbOff() || !g_shPsComp || !LogQuota(&s_q, 40)) return;  // v3.45 RB345: + no read back on this backend
    cx.k = ShCompC1(ShTfAlpha(tf), pct);
    for (k = 0; k < SH_PX_N; ++k) src[k] = (DWORD)kShPxA[k] << 24;
    if (!PxCheck(dev, src, 0xFFFFFFFFu, ShPxDraw, &cx, &pr, &why)) {
        if (why != kRbQuiet) LogV("shadows: pixel check (%s, darkness %08lX at Darkness %d %%) not run (%s)", path, tf, pct, why);  // v3.45 RB345
        return;
    }
    for (k = 0; k < SH_PX_N; ++k) g[k] = (unsigned char)(pr.dst[k][PXC_MID] >> 8);  // v3.47 PX347: the cell's centre
    ShPxAsked(t);
    v = ShPxVerdictT(cx.k, g, t, a, &off);
    PxSrcText(st, sizeof st, &pr, src, 0xFF000000u, &bad);
    if (v != SHX_OK && bad) {  // v3.47 PX347: the source held other coverage: the shader judged on what it held
        for (k = 0; k < SH_PX_N; ++k) tR[k] = (float)PxByte(pr.src[k], 24) / 255.0f;
        vR = ShPxVerdictT(cx.k, g, tR, aR, &offR);
        if (vR == SHX_OK) v = vR, off = offR, memcpy(t, tR, sizeof t), basis = 1;
    }
    LogV("shadows: pixel check (%s, darkness %08lX at Darkness %d %%) %s: a fixed test draw through the composite, read back: full "
         "cover alpha %.3f, asked %.3f (lets %.0f %% of the light through); 3/4 cover %.3f (asked %.3f), half %.3f (asked %.3f), none %.3f: %s",
         path, tf, pct, v == SHX_OK ? "ok" : "FAILED", a[0], ShCompShaderA(t[0], cx.k), 100.0f * ShLightThrough(a[0]), a[1],
         ShCompShaderA(t[1], cx.k), a[2], ShCompShaderA(t[2], cx.k), a[3], kShPxWhy[v]);
    LogV("shadows: pixel check (%s, Darkness %d %%): K %.3f, coverage asked %.3f / %.3f / %.3f / %.3f, read at the cells' centres "
         "(green %u / %u / %u / %u); %s%s", path, pct, cx.k, (float)kShPxA[0] / 255.0f, (float)kShPxA[1] / 255.0f,
         (float)kShPxA[2] / 255.0f, (float)kShPxA[3] / 255.0f, g[0], g[1], g[2], g[3], st,
         basis ? "; the \"asked\" values are the sim on the source as read back" : "");  // v3.47 PX347
    sprintf_s(who, sizeof who, "shadows %s", path);
    PxAlignLog(who, &pr, 8);  // v3.47 PX347
    if (v != SHX_OK)
        LogAnomaly("shadows", "pixel check (%s, Darkness %d %%): %s (full cover alpha %.3f, asked %.3f)", path, pct, kShPxWhy[v], a[0], cx.k);
}
// v3.42 SM342: the game's own stock draw at the Darkness: stage 0's ALPHAARG1 DIFFUSE -> TFACTOR and TEXTUREFACTOR's alpha
// = ShStockDarkByte for this one call, both put back right after. 1 = drawn (*hr its result); 0 = the caller forwards
// the call untouched (100 %, no darkness this frame, a state not as ShadowCompositeDraw sets it, a failed query).
static int ShStockDark(void *dev, DWORD type, UINT start, UINT count, HRESULT *hr)
{
    DWORD tf = 0, op = 0, arg1 = 0;
    unsigned s8;
    if (!ShCallOk(type, start, count)) return 0;
    if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 60 /*TEXTUREFACTOR*/, &tf)) || !ShTfAlpha(tf)) return 0;
    s8 = ShStockDarkByte(ShTfAlpha(tf), ShDarkNow());
    if (s8 >= 255u) return 0;
    if (FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, 0, 4 /*ALPHAOP*/, &op)) ||
        FAILED(((SpGetTSS_t)VT(dev, SPV_GETTSS))(dev, 0, 5 /*ALPHAARG1*/, &arg1)) || op != 4 /*MODULATE*/ || arg1 != 0 /*DIFFUSE*/) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3))
            LogAnomaly("shadows", "the composite's stage 0 alpha is op %lu arg1 %lu (MODULATE DIFFUSE expected): Darkness not applied, "
                       "the game's draw", op, arg1);
        return 0;
    }
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 60 /*TEXTUREFACTOR*/, (DWORD)s8 << 24 | (tf & 0xFFFFFFu));
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 3 /*TFACTOR*/);
    *hr = ((ShDP_t)VT(dev, SHV_DP))(dev, type, start, count);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, arg1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 60 /*TEXTUREFACTOR*/, tf);
    ShDarkNote(SH_STOCK, tf, s8);
    return 1;
}
static struct { int saved; FxSaved sv; void *rt, *ds; } g_shCtx;
static void ShCtxRelease(void)
{
    if (g_shCtx.rt) ((Release_t)VT(g_shCtx.rt, VT_RELEASE))(g_shCtx.rt);
    if (g_shCtx.ds) ((Release_t)VT(g_shCtx.ds, VT_RELEASE))(g_shCtx.ds);
    g_shCtx.rt = g_shCtx.ds = NULL;
}
// 1 = drawn by us; 0 = the caller draws the stock call
// v3.52 SH352 (D8): *why = the reason a composite is not soft (SHN_SOFT when drawn), *tfOut = the darkness it saw
static int ShDrawBody(void *dev, DWORD type, UINT start, UINT count, int edge, int cap, int *why, DWORD *tfOut)
{
    void *mask = NULL, *low;
    FxSurfDesc8 d;
    DWORD tf = 0, cwe = 0xF;
    const ShPlan *pl;
    int w, h, fail = SHF_NONE, ok, gate;
    *why = SHN_FAIL, *tfOut = 0;  // v3.52 SH352: until proven otherwise
    if (!ShCallOk(type, start, count)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("shadows", "0x55FE43 drew type %lu start %u count %u (stock: 4 0 4): left stock", type, start, count);
        *why = SHN_CALL;  // v3.52 SH352
        return 0;
    }
    ShDevCheck(dev);
    if (FAILED(((SpGetTex_t)VT(dev, SPV_GETTEX))(dev, 0, &mask)) || !mask) {
        *why = SHN_MASK;  // v3.52 SH352
        return 0;
    }
    if (mask != *(void *volatile *)0x12E7C40) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("shadows", "stage 0 holds %p, not the shadow mask %p: left stock", mask, *(void *volatile *)0x12E7C40);
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        *why = SHN_MASK;  // v3.52 SH352
        return 0;
    }
    if (FAILED(((ShTexDesc_t)VT(mask, 14))(mask, 0, &d)) || d.w < 2 * SH_RT_MIN || d.h < 2 * SH_RT_MIN ||
        FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 60 /*TEXTUREFACTOR*/, &tf))) {
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        ShFail(SHF_STATE, E_FAIL, "the mask's size / TEXTUREFACTOR");
        return 0;  // v3.52 SH352: SHN_FAIL
    }
    *tfOut = tf;  // v3.52 SH352
    w = (int)d.w, h = (int)d.h;
    if (ShTfAlpha(tf)) g_shLastTf = tf;
    if (cap) ShCapture(dev, mask, w, h, tf);
    if ((gate = ShNsGate(ShTfAlpha(tf), edge, g_shFailed)) != SHN_SOFT) {  // v3.52 SH352: the reason kept (was edge / failed / no darkness)
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        *why = gate;
        return 0;
    }
    if ((pl = ShPlanFor(edge, w, h)) == NULL) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("shadows", "no plan for %s at %dx%d (a size the chain holds): stock", kShEdgeName[edge], w, h);
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        *why = SHN_PLAN;  // v3.52 SH352
        return 0;
    }
    if (!ShShaders(dev)) fail = SHF_SHADER;
    else if (!ShChainReady(dev, pl)) fail = SHF_RT;
    if (fail == SHF_RT && ++g_shRtFails < 3) {  // a lost device, a full video memory for a moment: stock now, tried again
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        Log("shadows: our render targets could not be made (%d in a row): the stock edge this time, tried again", g_shRtFails);
        *why = SHN_RT;  // v3.52 SH352
        return 0;
    }
    if (fail) {
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        ShFail(fail, E_FAIL, "making our objects");
        return 0;  // v3.52 SH352: SHN_FAIL
    }
    g_shRtFails = 0;
    if (!FxSave(dev, &g_shCtx.sv, 3)) {
        ((Release_t)VT(mask, VT_RELEASE))(mask);
        ShFail(SHF_STATE, E_FAIL, "saving the device state");
        return 0;  // v3.52 SH352: SHN_FAIL
    }
    g_shCtx.saved = 1;
    cwe = g_shCtx.sv.rs[12];  // kFxRs[12] = COLORWRITEENABLE: the game's own mask for the composite
    if (FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &g_shCtx.rt)) || !g_shCtx.rt) g_shCtx.rt = NULL;
    if (FAILED(((GetSurf_t)VT(dev, SHV_GETDS))(dev, &g_shCtx.ds))) g_shCtx.ds = NULL;
    ok = g_shCtx.rt != NULL;
    low = ok ? ShRunChain(dev, mask, pl, (float)ShTfAlpha(tf) / 255.0f, g_shCtx.ds, &fail) : NULL;
    if (g_shCtx.rt && !ShTargetBack(dev, g_shCtx.rt, g_shCtx.ds, &g_shCtx.sv.vp)) ok = 0, fail = SHF_TARGET;
    {   // v3.42 SM342: at the Darkness (v3.43 SH343: one read of it; c1 read back at a new value and every SH_C1_EVERY-th)
        const int pct = (int)g_shDark, due = ShC1Due(ShC1Key(ShTfAlpha(tf), pct), g_shC1Key[1], ++g_shC1N);
        float got = -1.0f;
        if (ok && low && ShC1Key(ShTfAlpha(tf), pct) != g_shC1Key[1]) ShPxCheck(dev, 1, tf, pct);  // v3.44 CS344: a new value
        if (ok && low && !ShCompose(dev, pl, low, tf, (float)w, (float)h, -1e9f, 1e9f, cwe, pct, due ? &got : NULL)) fail = SHF_DRAW;
        if (ok && low && !fail) {
            ShDarkNote(SH_SOFT, tf, 0);
            if (due) ShC1Note(1, tf, pct, got);  // v3.43 SH343
        }
    }
    FxRestore(dev, &g_shCtx.sv);
    ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_shCtx.sv.vp);
    g_shCtx.saved = 0;
    ShCtxRelease();
    ((Release_t)VT(mask, VT_RELEASE))(mask);
    if (!ok || !low || fail) {
        ShFail(fail ? fail : SHF_STATE, E_FAIL, "the soft composite");
        return 0;
    }
    if (g_shLogN > 0) {
        --g_shLogN;
        LogV("shadows: composite %s at %dx%d, darkness %08lX, %d draws, frame %ld (%u this frame), stage %X room %X", kShEdgeName[edge],
             w, h, tf, pl->nsteps, g_fxFrame, g_shFrameN + 1, *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94);
    }
    *why = SHN_SOFT;  // v3.52 SH352
    return 1;
}
// v3.52 SH352 (D8): the Stock / off path's room count: its reason (no darkness, Stock, off), never a fault
static void ShStockTick(void *dev, int edge)
{
    __try {
        DWORD tf = 0;
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, 60 /*TEXTUREFACTOR*/, &tf))) tf = 0xFF000000u;  // unknown: counted as a shadow
        ShRoomTick(ShNsGate(ShTfAlpha(tf), edge, 1), tf, 0.0f);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
static HRESULT __stdcall ShDrawHook(void *dev, DWORD type, UINT start, UINT count)
{
    const ShDP_t dp = (ShDP_t)VT(dev, SHV_DP);
    const int edge = ShEdgeGet();
    LARGE_INTEGER t0, t1;
    HRESULT hr = S_OK;
    DWORD tf = 0;  // v3.52 SH352
    int cap, drawn = 0, why = SHN_FAIL;  // v3.52 SH352: + why
    cap = g_shPrevOn && g_fxPrevHook && g_shMenu && *(volatile int *)0x12E3E2C == 1 && g_shCapFrame != g_fxFrame;
    if ((edge == SH_STOCK || g_shFailed) && !cap) {  // Stock: no work (v3.42 SM342: but the Darkness below 100 %)
        ShStockTick(dev, edge);  // v3.52 SH352: counted in the room line with its reason (was not counted)
        if (g_shDark >= 100) return dp(dev, type, start, count);
        __try {
            if (ShStockDark(dev, type, start, count, &hr)) return hr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            static volatile LONG s_q;
            if (LogQuota(&s_q, 3)) LogAnomaly("shadows", "the Darkness on the stock draw faulted: the game's draw");
        }
        return dp(dev, type, start, count);
    }
    if (InterlockedExchange(&g_shInHook, 1)) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 3)) LogAnomaly("shadows", "the composite hook was entered again while it ran: stock draw");
        return dp(dev, type, start, count);
    }
    if (!g_shQf.QuadPart) QueryPerformanceFrequency(&g_shQf);
    QueryPerformanceCounter(&t0);
    BgPush(BGC_SHADOW);
    __try {
        drawn = ShDrawBody(dev, type, start, count, edge, cap, &why, &tf);  // v3.52 SH352: + why, tf
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        __try {  // put the game's target and states back before its own draw
            if (g_shCtx.saved) {
                if (g_shCtx.rt) ((ShSetRT_t)VT(dev, SHV_SETRT))(dev, g_shCtx.rt, g_shCtx.ds);
                FxRestore(dev, &g_shCtx.sv);
                ((Viewport_t)VT(dev, FXV_SETVP))(dev, &g_shCtx.sv.vp);
            }
            ShCtxRelease();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        g_shCtx.saved = 0;
        ShFail(SHF_FAULT, E_FAIL, "inside the composite hook");
        drawn = 0;
        why = SHN_FAIL;  // v3.52 SH352
    }
    BgPop();
    if (!drawn) {  // v3.42 SM342: the stock draw at the Darkness (Stock with the preview's capture, a soft fallback)
        int sd = 0;
        if (g_shDark < 100) {
            __try {
                sd = ShStockDark(dev, type, start, count, &hr);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                sd = 0;
            }
        }
        if (!sd) hr = dp(dev, type, start, count);
    }
    QueryPerformanceCounter(&t1);
    ShRoomTick(drawn ? SHN_SOFT : why == SHN_SOFT ? SHN_FAIL : why, tf,
               g_shQf.QuadPart ? (float)((double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)g_shQf.QuadPart) : 0.0f);  // v3.52 SH352: + the reason
    InterlockedExchange(&g_shInHook, 0);
    return hr;
}

// ---------------------------------------------------------------- v3.49 SH349: the Tremer's wall shadow at the Darkness
// USER_FEEDBACK_v348 E ("for soft shadows ... include the whole game not just player, allies, and enemies"). What the
// stock game draws as a shadow (BUILD_NOTES.md of staging/v349_feedback/w6_shadows has the evidence):
//   1. every real-time shadow: z-fail stencil volumes (ModelDrawShadow 0x55C860 -> 0x424E80 -> 0x425700) of every model
//      that has a shadow file (ModelSetShadowData 0x55C830): the player (Henry / Playable Eileen) and the weapon in hand
//      (the Sword of Obedience held and pinned too), the final boss room's spears (0x541FB0, stage 9 room 2),
//      Eileen and Cynthia as companions, every enemy type with a shadow file (Thermal skips the ghosts' at 0x52AF95),
//      ghosts, Room 302's hauntings (0x549C40, with their own shadow light 0x549B70), the Water Prison's turning prison
//      (0x4E99C0), event and in-engine cut-scene figures (actor passes 0x52AF60 / 0x52AFC0, and inline after one model in
//      five event scenes: 0x4DE412 0x4DEFE3 0x4DF323 0x4E1452 0x4EE3A2). They all end in ONE mask per composite
//      (ShadowCompositeDraw 0x55F660, the only reader of the stencil count) and so in ShDrawHook: Soft and the Darkness
//      have covered all of them since v3.40 / v3.42 (the room lines: "N composites ..., N soft");
//   2. the Tremer's wall shadow: a subtractive textured ribbon (shadows_sim.h ShRibbonAlpha), not in the mask: the
//      Darkness now applies to it (this hook); Soft cannot (its edge is its texture's, nothing to blur);
//   3. baked shadows: the rooms are drawn unlit, their shadows (furniture, doors, Room 302, props, items) are painted
//      into the textures and vertex colours; items and doors have no shadow file at all: not a draw Refined can soften
//      (texture work: SH4 HD Textures). Movies (FMV) are pictures. Echo Doubles' ground shadow is Refined's own soft disc.
// The site: 0x45F4CA `call 45F1E0` (E8 11 FD FF FF) in EffectDrawRibbonShadow 0x45F480, whose only callers are the
// Tremer's 0x48A6C4 / 0x48AA4C; eax = the colour there (`mov eax, [esp+2Ch]` at 0x45F4C3; 0x45F1E0 reads it at 0x45F1F9).
// -> `call hkShRibbon`: the alpha byte scaled, then a jump to 0x45F1E0. Not a Playable Eileen hook, not in a cheat-table
// span (ct_conflicts --span 0x45F4C3-0x45F4D0: none); Refined's other site in this effect is the flush's return 0x45F3FA
// (Menus43's world list: only read). [Shadows] TremerShadows=0: not installed (the game's own wall shadow).
static int g_shRibOn;                   // the hook is in
static DWORD g_shRibRoomKey = 0xFFFFFFFFu;  // the room the counts belong to (logged when it changes: ShFrameEnd)
static unsigned g_shRibRoomN, g_shRibRoomDark, g_shRibLastKey[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
static DWORD g_shRibA8Min = 255u, g_shRibA8Max;  // the alpha bytes seen this room (asked)
static const DWORD g_shRibFn = 0x45F1E0;
static DWORD ShRoomKeyNow(void) { return (DWORD)*(volatile int *)0xFD5A90 << 16 | ((DWORD)*(volatile int *)0xFD5A94 & 0xFFFF); }
// the counted room's line (only a room with a Tremer's wall shadow has counts), then the counts reset
static void ShRibbonRoomLog(void)
{
    if (g_shRibRoomKey != 0xFFFFFFFFu && g_shRibRoomN)
        LogV("shadows: room %lX:%02lX: %u Tremer wall shadow draw(s), %u at the Darkness (%d %%; alpha %02lX..%02lX asked)",
             g_shRibRoomKey >> 16, g_shRibRoomKey & 0xFFFF, g_shRibRoomN, g_shRibRoomDark, (int)g_shDark, g_shRibA8Min, g_shRibA8Max);
    g_shRibRoomKey = 0xFFFFFFFFu;
    g_shRibRoomN = g_shRibRoomDark = 0;
    g_shRibA8Min = 255u, g_shRibA8Max = 0u;
}
static DWORD __cdecl ShRibbonC(DWORD c)
{
    __try {
        const int pct = (int)g_shDark;
        const unsigned a8 = c >> 24;
        const DWORD key = ShRoomKeyNow();
        const int slot = a8 == 0x30;  // the once-per-Darkness line: the stock 0x30 (slot 1), and a fading one (slot 0, any alpha)
        const unsigned lk = ShRibbonKey(slot ? a8 : 0u, pct);
        DWORD out = c;
        if (key != g_shRibRoomKey) ShRibbonRoomLog(), g_shRibRoomKey = key;  // the last room's line first (a room change before ShFrameEnd saw it)
        ++g_shRibRoomN;
        if (a8 < g_shRibA8Min) g_shRibA8Min = a8;
        if (a8 > g_shRibA8Max) g_shRibA8Max = a8;
        if (pct < 100) {
            out = (DWORD)ShRibbonColour(c, pct);
            ++g_shRibRoomDark;
        }
        if (lk != g_shRibLastKey[slot]) {
            static volatile LONG s_q;
            g_shRibLastKey[slot] = lk;
            if (LogQuota(&s_q, 20))
                LogV("shadows: a Tremer's wall shadow drawn at Darkness %d %%: its alpha %02X -> %02X (%.0f %% of the game's darkening; "
                     "subtractive ribbon 0x45F1E0, its edge is its texture's)", pct, a8, (unsigned)(out >> 24),
                     a8 ? 100.0f * ShRibbonDark(out >> 24) / ShRibbonDark(a8) : 100.0f);
        }
        return out;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return c;
    }
}
#include "shadows_rib.h"  // hkShRibbon: 0x45F4CA, was `call 45F1E0` (eax = the colour; args on the stack); test\shribbon_test.c
// ShFrameEnd: a room's line once it is left (only for rooms with a Tremer's wall shadow)
static void ShRibbonRoomEnd(void)
{
    if (!g_shRibOn || g_shRibRoomKey == 0xFFFFFFFFu || ShRoomKeyNow() == g_shRibRoomKey) return;
    ShRibbonRoomLog();
}
static void ShRibbonInstall(void)
{
    static const Site kRib = { 0x45F4C3, { 0x8B, 0x44, 0x24, 0x2C, 0x52, 0x57, 0x56, 0xE8, 0x11, 0xFD, 0xFF, 0xFF, 0x83, 0xC4, 0x0C }, 15 };
    static const Site kRibUse = { 0x45F1F9, { 0x8B, 0xD8, 0xC1, 0xE8, 0x18 }, 5 };  // 45F1E0: mov ebx, eax; shr eax, 18h (the colour)
    if (!IniInt("Shadows", "TremerShadows")) {  // kIniKeys: default 1 (a missing key is added)
        Log("shadows: [Shadows] TremerShadows=0: the Tremers' wall shadows stay the game's own (the Darkness does not apply to them)");
        return;
    }
    if (!SiteOk(&kRib) || !SiteOk(&kRibUse)) {
        Log("shadows: the Tremer's wall shadow draw %s is not stock (another mod?): the Darkness does not apply to it",
            !SiteOk(&kRib) ? "0045F4C3" : "0045F1F9");
        return;
    }
    if (!PokeBranch(0x45F4CA, 0xE8, (const void *)hkShRibbon, 5)) {
        Log("shadows: 0045F4CA could not be written: the Tremers' wall shadows stay the game's own");
        return;
    }
    g_shRibOn = 1;
}

// ---------------------------------------------------------------- the Shadows page's preview (Options frames)
static int ShMode(int *why)
{
    ShPrevIn in;
    in.previewOn = g_shPrevOn && !PvPreviewOff();  // v3.58 PV358: Effects > Preview Off: no split preview either (the game's background)
    in.backdropHook = g_fxPrevHook;
    in.failed = g_shFailed;
    in.pausedGame = FxGameplay() && *(volatile int *)0xFD5C20 == 2;
    in.haveCopies = g_shCopies;
    in.copiesFrame = (int)g_shCapFrame;
    in.pauseFrame = (int)g_shPauseFrame;
    return ShPreviewMode(&in, why);
}
// 0x526A53, before the page frame (screenfx.inc FxBackdropC on this page): the backdrop. 1 = drawn (the hallway is skipped)
static int ShBackdrop(void *dev)
{
    int why, mode;
    void *tex;
    if (!dev || !FxOnBackBuffer(dev)) return 0;
    ShDevCheck(dev);
    mode = ShMode(&why);
    if (mode != g_shModeWas || why != g_shWhyWas) {
        LogV("shadows: the Shadows page's preview: %s (%s)", mode == SHP_SCENE ? "the paused scene, split Stock | the chosen edge" :
             mode == SHP_CARD ? "a figure's shadow, split Stock | the chosen edge" : "none", kShWhy[why]);
        g_shModeWas = mode, g_shWhyWas = why;
    }
    if (mode == SHP_SCENE && g_shClean.tex) {
        FxDrawPic(dev, g_shClean.tex, (UINT)g_shClean.w, (UINT)g_shClean.h);
        return 1;
    }
    if (mode == SHP_CARD && (tex = FxPrevLoad(dev)) != NULL) {
        FxDrawPic(dev, tex, g_fxPrevW, g_fxPrevH);
        return 1;
    }
    return 0;  // the stock hallway
}
// the figure's mask (the card) into g_shMaskRt: cleared, then the hard-edged triangles in TEXTUREFACTOR's colour
static int ShCardMask(void *dev, int w, int h, DWORD tf, void *rt, void *ds, const Viewport8 *vp)
{
    static float xy[SH_FIG_TRIS * 6];
    SpVtx v[SH_FIG_TRIS * 3];
    int n, i, ok;
    if (g_shCardOn && g_shCardW == w && g_shCardH == h && g_shCardTf == tf && g_shMaskRt.tex) return 1;
    g_shCopies = 0;  // v3.41 SH341: the mask target is reused for the figure (kept), not freed
    if (!ShRtMake(dev, &g_shMaskRt, w, h, 21, "the figure's mask")) return 0;
    if (!ShTarget(dev, &g_shMaskRt, ds)) {
        ShTargetBack(dev, rt, ds, vp);
        return 0;
    }
    ((Clear_t)VT(dev, VT_CLEAR))(dev, 0, NULL, 1 /*TARGET*/, 0, 1.0f, 0);
    n = ShFigure(0.5f * (float)w, (float)h * 184.0f / 448.0f, (float)h * 160.0f / 448.0f, xy);
    for (i = 0; i < n * 3; ++i) {
        v[i].x = xy[2 * i] - 0.5f, v[i].y = xy[2 * i + 1] - 0.5f, v[i].z = 0.0f, v[i].rhw = 1.0f, v[i].c = tf, v[i].u = v[i].v = 0.0f;
    }
    FxBase(dev);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 168 /*COLORWRITEENABLE*/, 0xF);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
    ok = SUCCEEDED(((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)n, v, sizeof(SpVtx)));
    if (!ShTargetBack(dev, rt, ds, vp)) ok = 0;
    if (ok) {
        g_shCardOn = 1, g_shCardW = w, g_shCardH = h, g_shCardTf = tf;
        LogV("shadows: the Shadows page's figure drawn (%dx%d mask, %d triangles, darkness %08lX)", w, h, n, tf);
    }
    return ok;
}
// a thin split line at x (light, with dark sides)
static void ShSplitLine(void *dev, float x, float h)
{
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
    ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
    ((SpSetTex_t)VT(dev, SPV_SETTEX))(dev, 0, NULL);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 0 /*DIFFUSE*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
    ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 27 /*ALPHABLENDENABLE*/, 1);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 19 /*SRCBLEND*/, 5 /*SRCALPHA*/);
    ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, 20 /*DESTBLEND*/, 6 /*INVSRCALPHA*/);
    FxRect(dev, 0x80000000u, x - 2.0f, 0.0f, x + 2.0f, h, 0, 0, 0, 0);
    FxRect(dev, 0xC0E0E0E0u, x - 0.5f, 0.0f, x + 0.5f, h, 0, 0, 0, 0);
}
// 0x526A53, after the backdrop (screenfx.inc FxBackdropFilter on this page): the split shadows, the line, the panels
static void ShPreviewDraw(void)
{
    void *dev = DEV;
    int why, mode, w, h, edge = ShEdgeGet(), ok = 1;
    DWORD tf;
    if (!dev || !FxOnBackBuffer(dev)) return;
    mode = ShMode(&why);
    if (mode == SHP_NONE) return;
    BgPush(BGC_SHADOW);
    __try {
        void *rt = NULL, *ds = NULL, *mask;
        FxSaved sv;
        const ShPlan *pl = NULL;
        float split;
        // v3.58 GX358: Shadows Off: the preview shows the game's Off (in play: the paused room's own rule, its frame without
        // the shadow; from the title: no figure's shadow), no split
        const int offHere = FxGameplay() ? !ShStockShadowsHere() : !ShRealOn();
        static int s_offWas = -1;
        static volatile LONG s_qOff;
        if (offHere != s_offWas) {
            if ((s_offWas >= 0 || offHere) && LogQuota(&s_qOff, 40))  // (bounded: a row held on the page flips it often)
                LogV("shadows: the Shadows page's preview: %s", offHere ? "Shadows Off: no real-time shadow here, as the game's Off (the "
                     "picture without it, no split)" : "the split shadows again (Shadows On, or a room the game keeps them in)");
            s_offWas = offHere;
        }
        if (mode == SHP_SCENE) w = g_shMaskRt.w, h = g_shMaskRt.h, tf = g_shCapTf;
        else w = (int)FxW(), h = (int)FxH(), tf = g_shLastTf ? g_shLastTf : 0x32000000u;  // stage 1's darkness until one is seen
        split = (float)(w / 2);
        if (!offHere && w >= 2 * SH_RT_MIN && h >= 2 * SH_RT_MIN && FxSave(dev, &sv, 3)) {  // v3.58 GX358: + !offHere
            if (FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt))) rt = NULL;
            if (FAILED(((GetSurf_t)VT(dev, SHV_GETDS))(dev, &ds))) ds = NULL;
            if (rt && (mode == SHP_SCENE ? g_shMaskRt.tex != NULL : ShCardMask(dev, w, h, tf, rt, ds, &sv.vp))) {
                mask = g_shMaskRt.tex;
                ShComposeStock(dev, mask, (float)w, (float)h, -1e9f, split, 255u);  // Stock: left of the line (v3.42 SM342: the game's own, 100 %)
                if (edge != SH_STOCK && !g_shFailed && (pl = ShPlanFor(edge, w, h)) != NULL && ShShaders(dev) && ShChainReady(dev, pl)) {
                    int fail = SHF_NONE;
                    void *low = ShRunChain(dev, mask, pl, (float)ShTfAlpha(tf) / 255.0f, ds, &fail);
                    if (!ShTargetBack(dev, rt, ds, &sv.vp)) ok = 0;
                    else if (low) {  // v3.42 SM342: at the Darkness (v3.43 SH343: c1 read back at a new value, logged)
                        const int pct = (int)g_shDark, due = ShC1Key(ShTfAlpha(tf), pct) != g_shC1Key[0];
                        float got = -1.0f;
                        if (due) ShPxCheck(dev, 0, tf, pct);  // v3.44 CS344: a new value (puts the back buffer back)
                        ok = ShCompose(dev, pl, low, tf, (float)w, (float)h, split, 1e9f, 7, pct, due ? &got : NULL);
                        if (ok && due) ShC1Note(0, tf, pct, got);
                    } else ok = 0;
                } else {
                    ShComposeStock(dev, mask, (float)w, (float)h, split, 1e9f, ShStockDarkByte(ShTfAlpha(tf), ShDarkNow()));  // Stock chosen (or soft shadows off): both halves stock (v3.42 SM342: the right one at the Darkness)
                }
                ShSplitLine(dev, split, (float)h);
            }
            FxRestore(dev, &sv);
            ((Viewport_t)VT(dev, FXV_SETVP))(dev, &sv.vp);
            if (rt) ((Release_t)VT(rt, VT_RELEASE))(rt);
            if (ds) ((Release_t)VT(ds, VT_RELEASE))(ds);
        }
        if (!ok) {
            static volatile LONG s_q;
            if (LogQuota(&s_q, 3)) Log("shadows: the Shadows page's preview could not draw the chosen edge (the page keeps working)");
        }
        if (FxSave(dev, &sv, 0)) {  // the text panels (as Screen Effects; the middle of the screen stays clear for the preview)
            float x1 = 472.0f, y0 = 352.0f, y1 = 392.0f;
            FxBase(dev);
            FxTextPanels(dev, DescRowsBottom() + 2.0f);  // v3.44 DF344: light, behind the title / labels / values only (was FxPanel 28, 40, 484, 200)
            DescPanelBox(LP_SHADOWS, &x1, &y0, &y1);  // v3.54 OL34
            FxPanel(dev, 40.0f, y0, x1, y1);
            FxRestore(dev, &sv);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) LogAnomaly("shadows", "the Shadows page's preview faulted (logged once)");
    }
    BgPop();
}
// 0x55AC29, every frame (screenfx.inc FxAfterPost, before g_fxFrame moves on): the pause picture's frame, the copies'
// lifetime, the page's open / close
static void ShFrameEnd(void)
{
    ShKeepIn k;
    int why, shown;
    {   // v3.58 GX358: the stock Realtime Shadow switch (Effects > Shadows > Shadows): said at the start and when it changes
        static int s_was = -1;
        static volatile LONG s_q;
        const int on = ShRealOn();
        if (on != s_was) {
            if (s_was < 0 || LogQuota(&s_q, 40)) {
                if (on)
                    Log("shadows: Shadows On (the game's Realtime Shadow, cfg+0x19): real-time shadows wherever the game draws them");
                else
                    Log("shadows: Shadows Off (the game's Realtime Shadow, cfg+0x19, read by the game every frame: at once): no real-time "
                        "shadows, as the game's Off, except in the rooms the game keeps them (Room 302's living room, the Water Prison, "
                        "Hospital rooms 1 / 1F); here (stage %X room %X): %s; the Tremers' wall shadows stay (the game never hides them)",
                        *(volatile int *)0xFD5A90, *(volatile int *)0xFD5A94, ShStockShadowsHere() ? "kept" : "none");
            }
            s_was = on;
        }
    }
    if (!g_shOn) return;
    k.capture = *(volatile int *)0x12E3E2C;
    if (k.capture == 1) g_shPauseFrame = g_fxFrame;
    k.haveCopies = g_shCopies;
    k.gameplay = FxGameplay();
    k.phase = *(volatile int *)0xFD5C20;
    k.copiesFrame = (int)g_shCapFrame;
    k.pauseFrame = (int)g_shPauseFrame;
    shown = ShPageShown();
    __try {
        ShRibbonRoomEnd();  // v3.49 SH349
        const int paused = k.gameplay && (k.phase == 2 || k.capture != 0);
        if (!shown && ShCopiesFree(&k, &why)) ShDropCopies();  // v3.41 SH341: the contents end, the targets stay (was freed)
        if (!shown && g_shCardOn) ShDropCopies();
        if (g_shWasPaused && !paused && !shown) ShPauseEnded();  // v3.41 SH341: kept unless the address space runs low
        g_shWasPaused = paused;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (shown != g_shPageWas) {
        LogV("shadows: the Shadows page %s (Edge %s)", shown ? "opened" : "closed", kShEdgeName[ShEdgeGet()]);
        g_shPageWas = shown;
        if (!shown) g_shModeWas = g_shWhyWas = -1;
        g_shC1Key[shown ? 0 : 1] = 0xFFFFFFFFu;  // v3.43 SH343: the page's first draw / play's first composite after it log their c1
    }
}

// ---------------------------------------------------------------- v3.41 SH341: the Screen Effects page's pause picture
// FxCaptureClean (0x4EFBFD, the capture frame, before any filter: the frame is still clean) asks for it instead of the
// read back (FxGrabClean): a CopyRects of the back buffer (MSAA: the wrapper resolves, as for the preview's copy) into a
// render target of ours, made once and kept (released before a reset, as all of ours). 1 = done (seq = the picture's
// g_fxPausePicSeq); 0 = not here (no hooks, a refused format, a failure): screenfx.inc reads it back as before.
static int ShPauseGrab(void *dev, LONG seq)
{
    void *rt = NULL;
    FxSurfDesc8 d;
    HRESULT hr = E_FAIL;
    LARGE_INTEGER t0, t1;
    float msMake = 0.0f, msCopy = 0.0f;
    int ok = 0, made = 0;
    if (!g_shOn || !dev || g_shPauseFails >= 2 || !FxOnBackBuffer(dev)) return 0;
    if (!g_shQf.QuadPart) QueryPerformanceFrequency(&g_shQf);
    __try {
        ShDevCheck(dev);
        g_shPauseSeq = -1;
        if (SUCCEEDED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) && rt) {
            QueryPerformanceCounter(&t0);
            if (SUCCEEDED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) && (d.fmt == 21 || d.fmt == 22) && d.w >= 64 && d.h >= 64) {
                made = !(g_shPause.tex && g_shPause.w == (int)d.w && g_shPause.h == (int)d.h && g_shPause.fmt == d.fmt);
                if (ShRtMake(dev, &g_shPause, (int)d.w, (int)d.h, d.fmt, "the pause picture")) {
                    msMake = ShMsSince(&t0);
                    QueryPerformanceCounter(&t1);
                    hr = ((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_shPause.surf, NULL);
                    msCopy = ShMsSince(&t1);
                    ok = SUCCEEDED(hr);
                }
            }
            ((Release_t)VT(rt, VT_RELEASE))(rt);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
    }
    if (ok) {
        g_shPauseSeq = seq;
        g_shPauseFails = 0;
        LogV("screen effects: the pause picture kept on the GPU for the Screen Effects page: %dx%d (%.1f MB; %s %.2f ms, copied %.2f ms; "
             "nothing read back)", g_shPause.w, g_shPause.h, ShMb(ShPicBytes(g_shPause.w, g_shPause.h)),
             made ? "target made" : "target kept from an earlier pause:", msMake, msCopy);
    } else {
        ++g_shPauseFails;
        Log("screen effects: the pause picture could not be kept on the GPU (hr %08lX%s): read back from the GPU as before%s",
            (unsigned long)hr, rt ? "" : ", no render target", g_shPauseFails >= 2 ? " (from now on)" : "");
    }
    return ok;
}
// FxPauseTex: this pause's GPU picture (NULL: none, or not this pause's)
static void *ShPauseTex(void *dev, LONG seq, UINT *w, UINT *h)
{
    if (!g_shPause.tex || g_shPauseSeq < 0 || g_shPauseSeq != seq || dev != g_shDev) return NULL;
    *w = (UINT)g_shPause.w, *h = (UINT)g_shPause.h;
    return g_shPause.tex;
}

// ---------------------------------------------------------------- install (Init, before the Options tables)
static void ShadowsInstall(void)
{
    static const Site kDraw = { 0x55FE3C, { 0x6A, 0x04, 0x6A, 0x00, 0x6A, 0x04, 0x50, 0xFF, 0x91, 0x18, 0x01, 0x00, 0x00 }, 13 };
    static const Site kReset = { 0x402E0C, { 0x56, 0xE8, 0xEE, 0xE1, 0xFF, 0xFF, 0xE8, 0x59, 0xFE, 0xFF, 0xFF }, 11 };  // push esi; call 401000; call 402C70
    static const Site kRet = { 0x401000, { 0xC3 }, 1 };
    static const BYTE kStockDraw[6] = { 0xFF, 0x91, 0x18, 0x01, 0x00, 0x00 };
    char buf[32];
    int v, wasSofter = 0;
    IniStr("Shadows", "Edge", buf, sizeof buf);
    v = ShParseEdge(buf, &wasSofter);
    if (v < 0) {
        if (buf[0]) Log("shadows: [Shadows] Edge=%s is not a value (Stock, Soft): Stock", buf);
        v = SH_STOCK;
    }
    if (wasSofter)  // v3.41 SH341: v3.40's Softer (the ini keeps it until the row is changed)
        Log("shadows: [Shadows] Edge=%s (v3.40) reads as Soft: v3.41 has one soft edge, twice as wide as v3.40's Softer", buf);
    g_shEdge = v;
    {   // v3.42 SM342: [Shadows] Darkness (percent; a value of the ini kept as it is, the row shows the nearest)
        int pct;
        IniStr("Shadows", "Darkness", buf, sizeof buf);
        if (!ShParseDark(buf, &pct)) Log("shadows: [Shadows] Darkness=%s is not a percentage 0..100: %d", buf, SH_DARK_DEF);
        g_shDark = pct;
        Log("shadows: Darkness %d %% (of the game's inside darkness, after Playable Eileen's ShadowDarkness; Stock and Soft%s)", pct,
            pct != kShDarkPct[ShDarkIndex(pct)] ? "; not one of the row's values: the row shows the nearest" : "");
    }
    GetPrivateProfileStringA("Shadows", "SoftPx", "", buf, sizeof buf, g_iniPath);
    if (buf[0]) g_shSoftPx = ShClampPx((float)atof(buf), SH_SOFT_PX720);
    GetPrivateProfileStringA("Shadows", "SofterPx", "", buf, sizeof buf, g_iniPath);
    if (buf[0]) Log("shadows: [Shadows] SofterPx=%s is no longer read (v3.41: one soft edge; SoftPx sets its width, %.1f px at 720 lines)", buf, g_shSoftPx);
    ShPlanInit();  // v3.41 SH341: the chain's width table (about 10 ms, here rather than at the first soft composite)
    g_shPrevOn = GetPrivateProfileIntA("Shadows", "Preview", 1, g_iniPath) != 0;
    g_shLogN = GetPrivateProfileIntA("Shadows", "Log", 0, g_iniPath) ? 120 : 0;
    if (!SiteOk(&kDraw) || !SiteOk(&kReset) || !SiteOk(&kRet)) {
        Log("shadows: %s is not stock (another mod?): no Shadow Edge (the game's own shadows; [Shadows] Edge=%s not used)",
            !SiteOk(&kDraw) ? "the composite draw 0055FE3C" : "the device reset 00402E0C", kShEdgeName[v]);
        return;
    }
    if (!PokeBranch(0x55FE43, 0xE8, (const void *)ShDrawHook, 6)) {
        Log("shadows: 0055FE43 could not be written: no Shadow Edge");
        return;
    }
    if (!PokeBranch(0x402E0D, 0xE8, (const void *)hkShBeforeReset, 5)) {
        Poke(0x55FE43, kStockDraw, 6);  // all or nothing
        Log("shadows: 00402E0D could not be written: no Shadow Edge (0055FE43 put back%s)",
            memcmp((const void *)0x55FE43, kStockDraw, 6) ? ": FAILED, the draw forwards unchanged" : "");
        return;
    }
    g_shOn = 1;
    Log("shadows: hooks in place (the composite's draw 0055FE43, the device reset 00402E0D); Edge %s; Soft %.1f px at 720 lines, "
        "scaled with the height (down, blur and up in 2x steps); preview %s", kShEdgeName[v], g_shSoftPx,
        !g_shPrevOn ? "off ([Shadows] Preview=0)" : g_fxPrevHook ? "on (the page backdrop 0x526A53)" : "off (no page backdrop hook)");
    ShRibbonInstall();  // v3.49 SH349: after the composite's hooks (on its own: a failure here leaves them in)
    Log("shadows: the edge and the Darkness cover every shadow the game draws: the real-time shadows of every caster in one stencil "
        "mask a composite (the player and the weapon in hand, Eileen, Cynthia, enemies, ghosts, Room 302's hauntings, the boss room's "
        "spears, event and cut-scene "
        "figures), Edge and Darkness; the Tremers' wall shadows: %s; the shadows baked into the room textures (furniture, doors, "
        "items, Room 302) are not drawn as shadows (left as painted); where the game's darkness is 0 (stage 4 room 0x1F, "
        "cut-scene 0x43) no real-time shadow shows, with any edge: nothing to soften there",  // v3.52 SH352
        g_shRibOn ? "the Darkness (hook 0045F4CA; their edge is their texture's)" : "the game's own (no hook)");  // v3.49 SH349
}
