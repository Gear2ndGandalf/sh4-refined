// v3.31 DEPTH331: the player's (and the companions') true depth, restored right before Death Presence and the Echo
// shadows draw (user v3.30 item 7: "in most areas ... Death Presence and Echo Doubles render over the player").
// staging/v331_feedback/w3_depth/PLAN.md. Included from sh4windowmod.c after screenfx.inc; no exe patch.
//
// Why: the room / body pass stores z = P22 + P32 / w (KB 147). In every room with lights the light grid (0x433F00 ->
// 0x433C70, drawn at 0x4EFBDC, before our 0x4EFBFD hook) flushes 32 full-screen strips through 0x564210 with GS TEST
// 0x3000D (ZTE, ZTST ALWAYS) at GS z 10000 and without the 0x137F978 "no z write" flag the other effects set, so the
// port's 0x404070 writes depth 10000 x 2^-24 = 0.000596 (a wall at view depth ~27 000) over the whole screen. Our
// sprites and shadows then pass the depth test over her body. Outdoor rooms without such lights (Building World 2nd
// stage 5 room 4) keep the true depth.
// What: her body (and her weapon, the escort Eileen, Cynthia), as collected at 0x52AEB5 this frame, drawn once more
// through 0x55C650 with the draw-call override: no colour, Z test GREATEREQUAL, Z write on; the stock pose is reused
// ([model + 0xE0] NULLed for the call: no rebuild, so Playable Eileen's springs do not step again). Where a 2D layer
// overwrote her depth it comes back (her z is larger than 0.000596 nearer than ~27 000 units); where it is intact the
// redraw ties or loses and changes nothing.
// Cost: one extra body draw a frame per restored body (player + weapon, plus each companion in view), no colour writes,
// only on frames where Death Presence or Echo draws; logged per room. Hidden ini key: [ScreenEffects]
// PlayerDepthRestore=0 turns it off (v3.30's look, for comparison).
#include "depthfix_sim.h"

#define DFX_MAX 6
typedef struct { BYTE *model; const BYTE *task; int kind; } DfxEnt;
static DfxEnt g_dfx[DFX_MAX];
static int g_dfxN;
static LONG g_dfxStamp = -1;     // the frame g_dfx was collected in
static LONG g_dfxAsk = -100;     // the last frame a restore was asked (the collector runs only shortly after one)
static Viewport8 g_dfxVp;        // the 3D viewport in force at the frame's first registry draw
static int g_dfxVpOk;
static DfxGate g_dfxGate;
static int g_dfxFaults;
static int g_dfxFirstLogged;
static volatile int g_dfxInOvr;  // (diagnostic) draw calls the override saw in this restore
static DWORD g_dfxRoom = 0xFFFFFFFFu;
static float g_dfxMsMax;
static int g_dfxKinds, g_dfxBodiesMax, g_dfxCallsMax;
static DWORD g_dfxGridFrames, g_dfxFrames;
static DWORD g_dfxFaultCode;
static void *g_dfxFaultAt;
static int DfxFaultNote(EXCEPTION_POINTERS *e)
{
    g_dfxFaultCode = e->ExceptionRecord->ExceptionCode;
    g_dfxFaultAt = e->ExceptionRecord->ExceptionAddress;
    return EXCEPTION_EXECUTE_HANDLER;
}

static BYTE *DfxPeWeapon(void);  // v3.31 DEPTH-331-001: Playable Eileen's own weapon model (echo.inc, EchoPeWeapon)
// 0x52AEB5 (screenfx.inc ThermDrawC), every registry model draw
static void DfxCollect(const BYTE *task, BYTE *model)
{
    BYTE *her, *wep, *eil, *cyn;
    int kind, i;
    if (!g_dfxGate.ready || !g_dfxGate.on || !model || !DfxCollectWanted(g_fxFrame, g_dfxAsk)) return;
    if (g_dfxStamp != g_fxFrame) {  // the first body of a frame: a new list, and the 3D viewport in force
        void *dev = DEV;
        g_dfxStamp = g_fxFrame;
        g_dfxN = 0;
        g_dfxVpOk = dev && SUCCEEDED(((Viewport_t)VT(dev, FXV_GETVP))(dev, &g_dfxVp)) && g_dfxVp.w >= 16 && g_dfxVp.h >= 16;
    }
    her = *(BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    wep = *(BYTE *const volatile *)(0x10A0F20 + 0x340);
    if (model != wep) {  // v3.31 DEPTH-331-001: or Playable Eileen's own weapon (object 0xFCE5B0, validated)
        BYTE *const pw = DfxPeWeapon();
        if (pw && model == pw) wep = pw;
    }
    eil = *(BYTE *const volatile *)0xFCE6B4;
    cyn = *(BYTE *const volatile *)0xFD5A50;
    kind = DfxKind(model, task, her, wep, eil, cyn);
    if (kind < 0 || g_dfxN >= DFX_MAX) return;
    for (i = 0; i < g_dfxN; ++i)
        if (g_dfx[i].model == model) return;
    g_dfx[g_dfxN].model = model;
    g_dfx[g_dfxN].task = task;
    g_dfx[g_dfxN].kind = kind;
    ++g_dfxN;
}
static int DfxStill(const DfxEnt *e)  // still the same body as at 0x52AEB5 (Playable Eileen can switch the player model)
{
    switch (e->kind) {
    case DFK_PLAYER: return e->model == *(BYTE *const volatile *)(0x10A0F20 + 0x1B0);
    case DFK_WEAPON: return e->model == *(BYTE *const volatile *)(0x10A0F20 + 0x340) || e->model == DfxPeWeapon();  // v3.31 DEPTH-331-001
    case DFK_EILEEN: return e->task && e->task == *(const BYTE *const volatile *)0xFCE6B4;
    case DFK_CYNTHIA: return e->task && e->task == *(const BYTE *const volatile *)0xFD5A50;
    }
    return 0;
}

// the override, at each of the renderer's draw calls while our redraw runs (after every state the game set): depth only
static float g_dfxC[6][4];
static void __cdecl DfxOverride(void *dev)
{
    static const DWORD kRs[][2] = { { 7, 1 } /*ZENABLE*/, { 23, 7 } /*ZFUNC GREATEREQUAL*/, { 14, 1 } /*ZWRITEENABLE*/,
                                    { 168, 0 } /*COLORWRITEENABLE*/, { 27, 0 } /*ALPHABLENDENABLE*/, { 52, 0 } /*STENCILENABLE*/,
                                    { 15, 1 } /*ALPHATESTENABLE*/, { 25, 5 } /*ALPHAFUNC GREATER*/, { 24, 1 } /*ALPHAREF*/ };
    int i;
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i][0], kRs[i][1]);
    // kPsHeat with every colour constant 0 and c5.w = 1: a = the texel's alpha, so cut-out hair keeps its shape
    ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, g_fxPs[FXT_HEAT]);
    ((FxPsc_t)VT(dev, FXV_SETPSC))(dev, 0, g_dfxC, 6);
    ++g_dfxInOvr;
}
// v3.31 MERGE: the depth redraws keep this frame's face morph like Echo's redraws (no second lock / upload of the morph
// vertex buffer per body: the likely v3.30 frame spike); defined in echo.inc (EchoMorphHold, [Gameplay] EchoMorphKeep)
#define DFX_MORPH_N 64
typedef struct { int *at; int v; } DfxMorphSv;  // = echo.inc's EchoMorphSv
static int DfxMorphHold(BYTE *m, DfxMorphSv *sv, int max);
static int DfxDrawOne(BYTE *m)  // the stock pose again: [m + 0xC0] as the stock draw left it, no skeleton rebuild
{
    void *const sk0 = *(void **)(m + 0xE0);
    DfxMorphSv sv[DFX_MORPH_N + 1];
    int ok = 1, nsv = 0;
    *(void **)(m + 0xE0) = NULL;
    __try {
        nsv = DfxMorphHold(m, sv, DFX_MORPH_N);  // v3.31 MERGE
        ((void(__cdecl *)(void *))(ULONG_PTR)g_fnModel)(m);
    } __except (DfxFaultNote(GetExceptionInformation())) {
        ok = 0;
    }
    while (nsv > 0) --nsv, *sv[nsv].at = sv[nsv].v;  // v3.31 MERGE: the morph counts back
    *(void **)(m + 0xE0) = sk0;
    return ok;
}
// the light grid of this frame (0x4EFBDC: `if (0x55DD80() && ![0xFD5C2C]) call 0x433F00`; 0x55DD80 counts the lights
// flagged for it, a read-only loop): for the room's log line
static int DfxGridRan(void)
{
    int n = 0;
    __try {
        n = ((int(__cdecl *)(void))0x55DD80)();
        if (*(volatile int *)0xFD5C2C) n = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        n = 0;
    }
    return n > 0;
}
static void DfxRoomLine(DWORD room)
{
    static int s_lines;
    static const char *const kName[DFK_N] = { "player", "weapon", "Eileen", "Cynthia" };
    char who[64];
    int k;
    const float p22 = *(volatile float *)(0x12E3ED0 + 4 * 10), p32 = *(volatile float *)(0x12E3ED0 + 4 * 14);
    const float c2z = g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c2[2] : 1.0f / 16777216.0f;
    const float c3z = g_haveConsts && g_native.c2[2] != 0.0f ? g_native.c3[2] : 0.0f;
    const float zg = DfxGsDepth(10000.0f, c2z, c3z);
    if (s_lines >= 200 || !g_dfxFrames) return;
    ++s_lines;
    {   // the kinds seen this room, comma-separated (4 names: always fits)
        int n = 0, w;
        who[0] = 0;
        for (k = 0; k < DFK_N; ++k)
            if (g_dfxKinds >> k & 1) {
                w = _snprintf_s(who + n, sizeof who - (size_t)n, _TRUNCATE, n ? ", %s" : "%s", kName[k]);
                if (w < 0) break;
                n += w;
            }
    }
    Log("depth fix: stage %lX room %lX: up to %d bod%s restored a frame (%s; %d draw call(s)), slowest %.2f ms; the light grid "
        "(0x433F00, TEST 0x3000D, depth %.6f = a wall at view depth %.0f) drew in %lu of %lu frames%s", room >> 16, room & 0xFFFF,
        g_dfxBodiesMax, g_dfxBodiesMax == 1 ? "y" : "ies", who[0] ? who : "none drawn", g_dfxCallsMax, g_dfxMsMax, zg,
        DfxViewDepthOf(zg, p22, p32), g_dfxGridFrames, g_dfxFrames,
        g_dfxGridFrames ? " (this room's depth was overwritten: restored where they are)" : " (true depth here: the redraw ties)");
}
// Once a frame, before Death Presence's or the Echo shadows' first draw. game: the game's states the model draw
// expects (GvPass's capture), NULL = the current ones. Every state, the index buffer and the viewport go back.
static void DepthFixFrame(void *dev, const FxSaved *game, int who, int look)
{
    static const DWORD kRs[] = { 24 /*ALPHAREF*/, 25 /*ALPHAFUNC*/, 9 /*SHADEMODE*/, 60 /*TEXTUREFACTOR*/ };
    FxSaved sv;
    DWORD rs[4];
    void *ib = NULL;
    UINT base = 0;
    LARGE_INTEGER t0, t1, f;
    int i, ok = 1, bad = 0, bodies = 0, calls;
    if (!dev) return;
    if (g_dfxGate.ready && g_dfxGate.on) g_dfxAsk = g_fxFrame;  // (the collector runs from the next frames on)
    {   // a new room: the last one's line
        const DWORD rk = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF);
        if (rk != g_dfxRoom) {
            if (g_dfxRoom != 0xFFFFFFFFu) DfxRoomLine(g_dfxRoom);
            g_dfxRoom = rk;
            g_dfxMsMax = 0.0f;
            g_dfxKinds = g_dfxBodiesMax = g_dfxCallsMax = 0;
            g_dfxGridFrames = g_dfxFrames = 0;
        }
    }
    if (!DfxGateRun(&g_dfxGate, g_fxFrame, who, look)) return;
    if (g_dfxStamp != g_fxFrame || !g_dfxN || g_dfxFaults >= 3) return;
    if (!FxShader(dev, FXT_HEAT)) return;  // (FxShader logs a failure once)
    QueryPerformanceCounter(&t0);
    ++g_dfxFrames;
    if (DfxGridRan()) ++g_dfxGridFrames;
    if (!FxSave(dev, &sv, 6)) return;
    for (i = 0; i < 4; ++i)
        if (FAILED(((SpGetRS_t)VT(dev, SPV_GETRS))(dev, kRs[i], &rs[i]))) bad = 1;
    if (bad || FAILED(((GvGetIdx_t)VT(dev, FXV_GETIDX))(dev, &ib, &base))) {  // AddRef'd on success
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        FxRestore(dev, &sv);
        return;
    }
    memset(g_dfxC, 0, sizeof g_dfxC);
    g_dfxC[5][3] = 1.0f;
    g_dfxInOvr = 0;
    __try {
        ((Viewport_t)VT(dev, FXV_SETVP))(dev, g_dfxVpOk ? &g_dfxVp : game ? (Viewport8 *)&game->vp : &sv.vp);  // the 3D viewport
        sv.vpSet = 1;
        FxReapply(dev, game ? game : &sv);  // the game's states (its renderer sets per material only what it changes)
        for (i = 0; i < g_dfxN && ok; ++i) {
            if (!DfxStill(&g_dfx[i])) continue;
            InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, (void *)DfxOverride);
            ok = DfxDrawOne(g_dfx[i].model);
            InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
            if (ok) ++bodies, g_dfxKinds |= 1 << g_dfx[i].kind;
        }
    } __finally {
        InterlockedExchangePointer((void *volatile *)&g_fxDipOvr, NULL);
        ((GvSetIdx_t)VT(dev, FXV_SETIDX))(dev, ib, base);
        if (ib) ((Release_t)VT(ib, VT_RELEASE))(ib);
        for (i = 0; i < 4; ++i) ((SpSetRS_t)VT(dev, SPV_SETRS))(dev, kRs[i], rs[i]);
        FxRestore(dev, &sv);
    }
    calls = g_dfxInOvr;
    if (!ok) {
        ++g_dfxFaults;
        Log("depth fix: a body redraw faulted (code %08lX at %p)%s", g_dfxFaultCode, g_dfxFaultAt,
            g_dfxFaults >= 3 ? "; the depth fix is off for this session" : "");
    }
    QueryPerformanceCounter(&t1);
    if (QueryPerformanceFrequency(&f) && f.QuadPart) {
        const float ms = (float)((double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f.QuadPart);
        if (ms > g_dfxMsMax) g_dfxMsMax = ms;
    }
    if (bodies > g_dfxBodiesMax) g_dfxBodiesMax = bodies;
    if (calls > g_dfxCallsMax) g_dfxCallsMax = calls;
    if (!g_dfxFirstLogged && bodies) {
        g_dfxFirstLogged = 1;
        Log("depth fix: the player's depth restored before %s draws (%d bod%s, %d draw call(s), look %d)",
            who == DFW_ECHO ? "the Echo shadows" : "Death Presence", bodies, bodies == 1 ? "y" : "ies", calls, look);
    }
}

// Init, after EchoInstall: needs the draw-call override (0x4292EF / 0x429338) and the model hook (0x52AEB5)
static void DepthFixInstall(void)
{
    const int on = GetPrivateProfileIntA("ScreenEffects", "PlayerDepthRestore", 1, g_iniPath) != 0;  // hidden key
    const int ready = g_fxOn && g_spInstalled && g_thermHooks && g_gvDipHooks;
    DfxGateInit(&g_dfxGate, on, ready);
    if (!on) Log("depth fix: [ScreenEffects] PlayerDepthRestore=0: Death Presence and Echo draw over the stored depth as in v3.30 (a test)");
    else if (!ready)
        Log("depth fix: not available (scene pass %s, HUD hook %s, model hook %s, draw-call override %s): as in v3.30",
            g_fxOn ? "in" : "off", g_spInstalled ? "in" : "off", g_thermHooks ? "in" : "off", g_gvDipHooks ? "in" : "off");
    else Log("depth fix: in (the player's and the companions' depth redrawn once a frame before Death Presence / Echo draw)");
}
