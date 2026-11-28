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
//
// v3.58 PV358 (staging/v358_feedback/w2_preview/PLAN.md; USER_FEEDBACK_v357, Effects Preview, Improvements, Issues):
// 1. [ScreenEffects] Preview Off / Stock / Live (default Live), a row on Effects: Off no backdrop on the Effects pages (the
//    game's own menu background; the Shadows page's split preview off too); Stock the stock picture (the file
//    SH4Refined_preview_stock.dds in the game folder: the user's F7 picture, placed by the installer, never inside the dll,
//    never written by F7) on the title menu and in a pause; Live the stock picture on the title menu, a pause's own scene
//    with its layers (v3.56). Without the file: preview.dds (Live), then SH4 Refined's own scene (one log line). v3.56's
//    hidden PreviewScene is read once and migrated to Preview=Live (the old key removed, logged). The pause's layers are
//    still kept under every value (a filter changed in Options during a pause lays them over the pause menu's picture).
// 2. [ScreenEffects] FilterStrength Low / Medium / High / Ultra (default High = v3.16 - v3.57's look, byte for byte), a row
//    on Effects (grey with Filter None): Low / Medium lay the plain frame (copied before the filter) back at 65 / 35 %;
//    Ultra scales each look's amounts x 1.35 (preview_sim.h PvFsLook); Thermal's bodies and Spirit Vision's ghosts / fires
//    stay full. v3.15's unread FilterStrength value (no marker FilterStrengthV) reads as High once.
// 3. The heat layer's bodies write their depth again (v3.56 drew them with no depth write: a body the close-camera fade
//    left without depth of its own showed its inside faces; the live Thermal has written it since v3.24).
//
// v3.59 PV359 (staging/v359_feedback/w1_preview/PLAN.md; USER_FEEDBACK_v358 QA 3 and QA 11):
// 1. F7 keeps the look layers with its picture. A press arms the next frame (its bodies and souls are collected at
//    0x52AEB5 / 0x4EE376 as in a pause's capture frame); there, at 0x4EFBFD, the clean picture is saved as before, then the
//    capture frame's own draws (PvLayersRun: each layer over black, copied, the clean frame drawn back from a GPU copy) make
//    the heat and spirit layers, which are read back, shrunk as the picture and written beside it: <picture>.heat /
//    <picture>.spirit (pvlayer_sim.h: runs of black + literal pixels, CRC, the picture's stamp from its DDS header). A
//    picture with no hot body / no soul leaves that layer out (an older file of that name is removed).
// 2. The Effects page lays the picture's own layer over a picture backdrop (the stock picture: Stock, and the title menu
//    under Live; PreviewImage) under Thermal / Spirit Vision, as it lays a pause's: loaded when asked (one layer, the
//    filter's: at most 1920 x 1080 x 4 bytes, MANAGED), released when the page closes; a file that does not fit the
//    picture (size, stamp, CRC, runs) is not used (one line). The user's stock picture of 2026-11-26 has no stamp: no layer.
// 3. The Shadows page shows a still picture alone whenever its backdrop is one (PvShStillBack: Stock anywhere; Live at the
//    title menu or in a pause whose frame drew no shadow; as the Effects page picks it): the user's screenshot showed the
//    figure's split shadow drawn over the stock picture's Eileen. The split only over a live paused game's own shadow (Live).
#include "preview_sim.h"

static int g_pvOn = 1;              // [ScreenEffects] PreviewLayers
// (v3.58 PV358: g_pvSceneOn is gone: [ScreenEffects] PreviewScene is migrated to Preview, PvInstallModes)
static int g_pvMode = PVM_DEFAULT;  // v3.58 PV358: [ScreenEffects] Preview (PVM_*)
static volatile LONG g_pvFs = PVS_DEFAULT;  // v3.58 PV358: [ScreenEffects] FilterStrength (PVS_*)
static int g_pvFsMarked;            // v3.58 PV358: the marker FilterStrengthV=358 is on disk (read at the start or written since)
static LONG g_snapPvMode = PVM_DEFAULT, g_snapPvFs = PVS_DEFAULT;  // v3.58 PV358: as Options opened
static int g_pvRows, g_pvRow0 = -1;  // v3.58 PV358: the two rows are on the Screen Effects origin page (from row g_pvRow0)
static char g_pvStockPath[MAX_PATH];  // v3.58 PV358: <game folder>\SH4Refined_preview_stock.dds
static void *g_pvStockTex, *g_pvStockDev;
static UINT g_pvStockW, g_pvStockH;
static int g_pvStockFailed;         // v3.58 PV358: no texture on this device (checked again at the next page opening)
static int g_pvStockHave = -1;      // v3.58 PV358: the file is there (-1 not checked yet)
static int g_pvStockLogged;         // v3.58 PV358: its missing / unreadable line written (once a state)
static ShRt g_pvFsRt;               // v3.58 PV358: Filter Strength's plain copy (Low / Medium; the back buffer's size and format)
static LONG g_pvFsArmed = -1;       // v3.58 PV358: the frame whose plain copy PvFsMix may lay back (once)
static int g_pvFsFails;             // v3.58 PV358: faults / failed copies (3: Low / Medium draw as High for the session)
static volatile LONG g_pvFsQ;
static int g_pvOffNoted = -1;       // v3.58 PV358: the page whose Preview Off line was written this opening
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
// g_pvColl (its own GvCollect logs nothing): screenfx.inc  // v3.58 PV358: g_pvNoZw gone (the heat layer writes depth)
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
// v3.59 PV359: F7's layers and the picture layers
static volatile LONG g_pvF7Frame = -10;  // the frame an F7 press armed (its bodies are collected in it); -10 none
static int g_pvF7Armed, g_pvF7Why = PVW_OK;  // a press is waiting for that frame; why the last press could not arm (PVW_*)
static DWORD g_pvStamp;             // the stamp of the picture FxPrevWrite wrote last (its layers carry it)
static ShRt g_pvF7Clean;            // F7's clean frame on the GPU (drawn back after the layers; released at once)
static volatile LONG g_pvQF7, g_pvQLay, g_pvQSh;
typedef struct { void *tex, *dev; int pic, kind, tried; UINT w, h; } PvLayTex;  // pic 0 the stock picture, 1 PreviewImage
static PvLayTex g_pvLay;            // the one picture layer loaded (the page's filter's), released when the page closes
static int g_pvLayLast[2][3];       // the last result logged (PVLR_* + 1) per picture and kind
static int g_pvLayOff;              // a picture layer's draw faulted: off for the session
static LONG g_pvShDrew = -1, g_pvShSeen = -10;  // the Shadows page: the frame it drew the still picture alone; its last frame
static int g_pvShWhat = -1;

// ---------------------------------------------------------------- settings (ScreenFxInstall)
// v3.58 PV358: a key as it is in the file (1 = there; "" and 0 when missing or the ini cannot be written: g_iniFallback)
static int PvIniRaw(const char *key, char *out, DWORD n)
{
    out[0] = 0;
    if (g_iniFallback) return 0;
    GetPrivateProfileStringA("ScreenEffects", key, "\x01", out, n, g_iniPath);
    if (!strcmp(out, "\x01")) {
        out[0] = 0;
        return 0;
    }
    return 1;
}
static int PvIniWrite(const char *key, const char *value)  // 1 = written (NULL removes the key)
{
    return !g_iniFallback && WritePrivateProfileStringA("ScreenEffects", key, value, g_iniPath) != 0;
}
static int PvStockCheck(void)  // v3.58 PV358: is the stock picture file there (cached until the next page opening)
{
    g_pvStockHave = g_pvStockPath[0] && GetFileAttributesA(g_pvStockPath) != INVALID_FILE_ATTRIBUTES;
    return g_pvStockHave;
}
// v3.58 PV358: [ScreenEffects] Preview (v3.56's PreviewScene migrated once) and FilterStrength (v3.15's value: High once)
static void PvInstallModes(void)
{
    char p[32], s[32], v[32], m[16];
    const int hp = PvIniRaw("Preview", p, sizeof p), hs = PvIniRaw("PreviewScene", s, sizeof s);
    int mode, fs, how, w;
    if (g_iniFallback) {  // (the ini cannot be written: its values as read at the start, nothing migrated)
        int known;
        IniStr("ScreenEffects", "Preview", p, sizeof p);
        g_pvMode = PvParseMode(p, &known);
        InterlockedExchange(&g_pvFs, PVS_DEFAULT);
        Log("screen effects: SH4Refined.ini cannot be written: Preview %s, Filter Strength High for this start", kPvModeName[g_pvMode]);
        return;
    }
    how = PvModeIni(hp ? p : NULL, hs ? s : NULL, &mode);
    g_pvMode = mode;
    switch (how) {
    case PVI_MISSING:
        if (PvIniWrite("Preview", kPvModeName[mode])) Log("settings: [ScreenEffects] Preview was missing, added as \"%s\"", kPvModeName[mode]);
        break;
    case PVI_BAD:
        w = PvIniWrite("Preview", kPvModeName[mode]);
        Log("screen effects: [ScreenEffects] Preview=%s is not a value (Off, Stock, Live): %s (%s)", p, kPvModeName[mode],
            w ? "written" : "NOT written");
        break;
    case PVI_MIGRATE:  // v3.56's hidden key alone: Scene and Picture both mean Live now (a pause: its scene; else the picture)
        w = PvIniWrite("Preview", kPvModeName[mode]);
        if (w) PvIniWrite("PreviewScene", NULL);
        Log("screen effects: [ScreenEffects] PreviewScene=%s (v3.56, hidden) is now Preview=%s (v3.58: the Effects row Preview; %s)", s,
            kPvModeName[mode], w ? "written, the old key removed" : "NOT written: the old key is read again at the next start");
        break;
    case PVI_STALE:
        PvIniWrite("PreviewScene", NULL);
        Log("screen effects: [ScreenEffects] PreviewScene=%s (v3.56) is not read any more: Preview=%s decides (the old key removed)", s,
            kPvModeName[mode]);
        break;
    }
    PvIniRaw("FilterStrength", v, sizeof v);
    g_pvFsMarked = PvIniRaw("FilterStrengthV", m, sizeof m) && !strcmp(m, PV_FS_MARK);
    how = PvFsIni(v[0] ? v : NULL, g_pvFsMarked ? m : NULL, &fs);
    InterlockedExchange(&g_pvFs, fs);
    if (how == PVF_MISSING || how == PVF_LEGACY || how == PVF_BAD) {  // High, then the marker (never the marker over a v3.15 value)
        const int wv = PvIniWrite("FilterStrength", kPvFsName[fs]);
        const int wm = wv && (g_pvFsMarked || PvIniWrite("FilterStrengthV", PV_FS_MARK));
        if (wm) g_pvFsMarked = 1;
        if (how == PVF_MISSING)
            Log("settings: [ScreenEffects] FilterStrength was missing, added as \"%s\"%s", kPvFsName[fs], wm ? "" : " (NOT written)");
        else if (how == PVF_BAD)
            Log("screen effects: [ScreenEffects] FilterStrength=%s is not a value (Low, Medium, High, Ultra): %s (%s)", v, kPvFsName[fs],
                wm ? "written" : "NOT written");
        else if (!_stricmp(v, kPvFsName[PVS_HIGH]))  // (High without the marker: a fresh ini's default, or a v3.15 High: the same look)
            LogV("screen effects: [ScreenEffects] FilterStrength=High: the marker FilterStrengthV=%s %s", PV_FS_MARK, wm ? "added" : "NOT written");
        else
            Log("screen effects: [ScreenEffects] FilterStrength=%s is v3.15's (unread since v3.16): now %s, the strength of v3.16 - v3.57 "
                "(%s)", v, kPvFsName[fs], wm ? "written with the marker FilterStrengthV=" PV_FS_MARK : "NOT written: read as High again at the next start");
    }
}
// v3.59 PV359: which layer files lie beside a picture (the start line; the files are checked when the page asks for them)
static const char *PvLayFiles(const char *pic)
{
    char p[MAX_PATH];
    const int h = PvlPath(pic, PVLK_HEAT, p, sizeof p) && GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    const int s = PvlPath(pic, PVLK_SPIRIT, p, sizeof p) && GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    return h && s ? " (with its heat and spirit layer files)" : h ? " (with its heat layer file)" : s ? " (with its spirit layer file)" :
           " (no layer files)";
}
static void PvInstall(void)
{
    char d[MAX_PATH];
    g_pvOn = IniInt("ScreenEffects", "PreviewLayers") != 0;
    if (_snprintf_s(d, sizeof d, _TRUNCATE, "%s%s%s", g_dir, g_dir[0] && g_dir[strlen(g_dir) - 1] == '\\' ? "" : "\\", PV_STOCK_FILE) < 0) d[0] = 0;
    strcpy_s(g_pvStockPath, sizeof g_pvStockPath, d);  // v3.58 PV358
    PvInstallModes();  // v3.58 PV358
    PvStockCheck();
    Log("screen effects: the Effects page's preview: Preview=%s (%s); the stock picture %s: %s%s; a pause keeps Thermal's heat and "
        "Spirit Vision's ghosts and soul fires for it: %s; Filter Strength %s", kPvModeName[g_pvMode],
        g_pvMode == PVM_OFF ? "no backdrop: the game's own menu background" : g_pvMode == PVM_STOCK ?
        "the stock picture on the title menu and in a pause" : "the title menu: the stock picture, else PreviewImage; a pause: its scene",
        g_pvStockPath[0] ? g_pvStockPath : "(path too long)", g_pvStockHave ? "present" : "missing",
        g_pvStockHave ? PvLayFiles(g_pvStockPath) : "",  // v3.59 PV359
        g_pvOn ? "on (PreviewLayers=1)" : "off (PreviewLayers=0)", kPvFsName[(unsigned)g_pvFs < PVS_N ? (int)g_pvFs : PVS_DEFAULT]);
}
// the Filter row's PreviewKey sentence: the F7 picture is not shown on the Effects page while the scene is
// v3.58 PV358: never baked into the texts any more; PvFilterDesc adds it where F7 changes what the page shows
static int PvF7Hidden(void) { return 1; }
static int PvPreviewOff(void) { return g_pvMode == PVM_OFF; }  // v3.58 PV358
static int PvIsStockPath(const char *path) { return PvSamePath(path, g_pvStockPath); }  // v3.58 PV358

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
    const int n = (g_pvHeat.tex != NULL) + (g_pvSpirit.tex != NULL) + (g_pvFsRt.tex != NULL) + (g_pvF7Clean.tex != NULL);  // v3.58 PV358: + the plain copy  // v3.59 PV359: + F7's clean copy
    PvRtFree(&g_pvHeat);
    PvRtFree(&g_pvSpirit);
    PvRtFree(&g_pvFsRt);  // v3.58 PV358
    PvRtFree(&g_pvF7Clean);  // v3.59 PV359
    g_pvFsArmed = -1;
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
        memset(&g_pvFsRt, 0, sizeof g_pvFsRt), g_pvFsArmed = -1;  // v3.58 PV358
        memset(&g_pvF7Clean, 0, sizeof g_pvF7Clean);  // v3.59 PV359
    }
}
static void PvDevCheck(void *dev)
{
    if (dev == g_pvDev) return;
    if (g_pvDev) PvBeforeReset();  // (an older device's targets: released, guarded)
    g_pvDev = dev;
    memset(&g_pvHeat, 0, sizeof g_pvHeat), memset(&g_pvSpirit, 0, sizeof g_pvSpirit), g_pvVram = 0;
    memset(&g_pvFsRt, 0, sizeof g_pvFsRt), g_pvFsArmed = -1;  // v3.58 PV358
    memset(&g_pvF7Clean, 0, sizeof g_pvF7Clean);  // v3.59 PV359
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
    in->f7 = g_pvF7Frame == g_fxFrame;  // v3.59 PV359: the frame an F7 press armed
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
    if (!g_pvOn || !model || (*(volatile int *)0x12E3E2C != 1 && g_pvF7Frame != g_fxFrame)) return;  // every other frame: this one read  // v3.59 PV359: + an F7 frame
    PvInNow(&in);
    m = PvCollectMask(&in);
    // a ghost the live Thermal hides (ThermDrawC skips its draw) is not posed / drawn in this frame: no glow redraw of it
    if ((m & PVL_SPIRIT) && !ThermHidesBody(task)) {  // v3.59 MERGE: screenfx.inc's one Thermal Vision hide rule
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
    if (!g_pvOn || !model || (*(volatile int *)0x12E3E2C != 1 && g_pvF7Frame != g_fxFrame)) return;  // v3.59 PV359: + an F7 frame
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
// 0x4EFBFD, the capture frame / an F7 frame (v3.59 PV359): the back buffer is still the clean frame. The layers of want are
// drawn over black, each copied into its target, then the clean picture is drawn back (pic, the back buffer's size). f7: pic
// is made here (a GPU copy of the back buffer into g_pvF7Clean; a pause has its own, ShPauseTex). Returns the PVL_* kept.
// v3.59 PV359: PvCapture's draw part, shared with F7 (the same draws, the same fault rules and counters)
typedef struct { int made, fault, saved, restored, copyBad; float w, h; } PvRun;
static int PvLayersRun(void *dev, void *pic, UINT pw, UINT ph, int want, int f7, PvRun *o)
{
    void *rt = NULL;
    FxSurfDesc8 d;
    FxSaved sv;
    int have = 0;
    const char *const who = f7 ? "F7's" : "the pause's";
    memset(o, 0, sizeof *o);
    __try {
        PvDevCheck(dev);
        if (!FxOnBackBuffer(dev) || FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) || !rt) rt = NULL;
        if (rt && (FAILED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) || (d.fmt != 21 && d.fmt != 22) || d.w < 64 || d.h < 64 ||
                   (!f7 && (d.w != pw || d.h != ph)))) {
            if (!f7) PvWhyNote(PVW_NOPIC);  // (the target is not the picture's size / format: nothing to restore from 1:1)
            ((Release_t)VT(rt, VT_RELEASE))(rt);
            rt = NULL;
        }
        if (rt && f7) {  // v3.59 PV359: the clean frame kept on the GPU, to be drawn back after the layers
            pic = NULL;
            if (PvRtMake(dev, &g_pvF7Clean, (int)d.w, (int)d.h, d.fmt, "F7's clean copy", &o->made) &&
                SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvF7Clean.surf, NULL)))
                pic = g_pvF7Clean.tex;
            if (!pic) {
                ((Release_t)VT(rt, VT_RELEASE))(rt);
                rt = NULL;
            }
        }
        if (rt) {
            o->w = (float)d.w, o->h = (float)d.h;
            if ((want & PVL_HEAT) && !PvRtMake(dev, &g_pvHeat, (int)d.w, (int)d.h, d.fmt, "pause's heat layer", &o->made)) want &= ~PVL_HEAT;
            if ((want & PVL_SPIRIT) && !PvRtMake(dev, &g_pvSpirit, (int)d.w, (int)d.h, d.fmt, "pause's spirit layer", &o->made)) want &= ~PVL_SPIRIT;
            o->saved = want && FxSave(dev, &sv, FX_NPSC);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        o->fault = 1;
    }
    if (!o->saved) {
        if (rt) ((Release_t)VT(rt, VT_RELEASE))(rt);
        if (o->fault && ++g_pvFails && LogQuota(&g_pvQFail, 5))
            Log("screen effects: %s look layers faulted before drawing (%d of %d before they are off)", who, g_pvFails, PV_FAILS_OFF);
        return 0;
    }
    InterlockedIncrement(&g_pvCapDraws);  // v3.56 MERGE: this frame's interval is not a play sample (budget, rate guard)
    __try {
        sv.vpSet = 1;
        if (want & PVL_HEAT) {
            PvFullVp(dev, &sv, o->w, o->h);
            PvBlack(dev, o->w, o->h);
            g_pvPrints = PvPrintsDraw(dev, &sv);
            FxBase(dev);
            // v3.58 PV358 (USER_FEEDBACK_v357: "only displaying the inside faces of her model"): the living bodies write their
            // depth here as in the live heat pass (v3.24 user item 1), so the near faces win where the close-camera fade left no
            // depth of the player's own (KB 128); v3.56 drew them without (g_pvNoZw) and showed her far, inner faces. The
            // depth written is the body's own (0.03 % nearer: the heat bias), only in this capture frame
            ThRedraw(dev, &sv, &g_pvBodies);
            if (g_pvBodies + g_pvPrints > 0) {
                if (SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvHeat.surf, NULL))) have |= PVL_HEAT;
                else o->copyBad = 1;
            }
        }
        if (want & PVL_SPIRIT) {
            PvFullVp(dev, &sv, o->w, o->h);
            PvBlack(dev, o->w, o->h);
            PvSpiritDraw(dev, &sv);
            if (g_pvGhosts + g_pvFires + g_pvAuras > 0) {
                if (SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvSpirit.surf, NULL))) have |= PVL_SPIRIT;
                else o->copyBad = 1;
            }
        }
    } __except (GvFaultNote(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
        o->fault = 1;
        InterlockedExchange(&g_thMode, 0);
        InterlockedExchange(&g_gvGlow, 0);
    }
    __try {  // always: the clean frame back (the pause copy at the frame's end and every pass after this one see it)
        PvFullVp(dev, &sv, o->w, o->h);
        o->restored = PvRestore(dev, pic, o->w, o->h);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        o->restored = 0;
    }
    __try {
        FxRestore(dev, &sv);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    if (!o->restored) {  // the frame may now hold a layer instead of the scene: never again this session
        g_pvFails = PV_FAILS_OFF;
        have = 0;
        LogAnomaly("screen effects", "the clean %s could not be drawn back after the look layers: this %s may show them; the layers "
                   "are off for this session", f7 ? "frame" : "pause picture", f7 ? "frame" : "pause's picture");
    } else if (o->fault || o->copyBad) {
        ++g_pvFails;
        if (LogQuota(&g_pvQFail, 5))
            Log("screen effects: %s look layers %s (%s%08lX at %p; %d of %d before they are off); the clean picture was drawn back", who,
                o->fault ? "faulted" : "could not be copied", o->fault ? "code " : "", o->fault ? g_gvFaultCode : 0UL, o->fault ? g_gvFaultAt : NULL,
                g_pvFails, PV_FAILS_OFF);
    }
    return have;
}
// 0x4EFBFD, the capture frame, right after FxCaptureClean (FxSceneFrame): the back buffer is still the clean frame
static void PvCapture(void)
{
    void *dev = DEV, *pic = NULL;
    PvCapIn in;
    PvRun o;
    UINT pw = 0, ph = 0;
    LARGE_INTEGER t0, t1, qf;
    int want, why, have;
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
    have = PvLayersRun(dev, pic, pw, ph, want, 0, &o);  // v3.59 PV359: the draw part (shared with F7)
    if (!o.saved) return;
    QueryPerformanceCounter(&t1);
    g_pvHave = have;
    g_pvSeq = have ? g_fxPausePicSeq : -1;
    g_pvWhyLast = -1;
    if (have && LogQuota(&g_pvQCap, 80)) {
        const double ms = qf.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)qf.QuadPart : 0.0;
        LogV("screen effects: the pause's look layers kept for the Effects page: heat %s (%d bod%s, %d footprint(s)), spirit %s (%d ghost(s), "
             "%d soul fire(s), %d aura sprite(s)); %dx%d, %s (the preview's %.1f MB), %.2f ms", have & PVL_HEAT ? "on" : "off", g_pvBodies,
             g_pvBodies == 1 ? "y" : "ies", g_pvPrints, have & PVL_SPIRIT ? "on" : "off", g_pvGhosts, g_pvFires, g_pvAuras, (int)o.w, (int)o.h,
             o.made ? "targets made" : "targets kept from an earlier pause", ShMb(g_pvVram), ms);
    }
}

// ---------------------------------------------------------------- v3.59 PV359: F7 keeps the layers with its picture
// FxPrevWrite (every picture written: F7, the first-pause capture): a fresh stamp for its DDS header; its layers carry it
static DWORD PvPicStampNext(void)
{
    LARGE_INTEGER q;
    DWORD s;
    QueryPerformanceCounter(&q);
    s = (GetTickCount() * 2654435761u) ^ q.LowPart ^ ((DWORD)GetCurrentProcessId() << 16) ^ (DWORD)q.HighPart;
    if (!s) s = 0x5EED;
    if (s == g_pvStamp) s ^= 0x00010001u;
    g_pvStamp = s;
    return s;
}
// FxPreviewKeyFrame (0x4EFBFD, a frame of play, the key just pressed): 1 = armed (the next frame collects its bodies and
// takes the picture with its layers); 0 = take the picture now, alone (no layers can be drawn: g_pvF7Why says why)
static int PvF7Arm(void)
{
    PvCapIn in;
    int why;
    PvInNow(&in);
    if (!PvF7CanLayer(&in, &why)) {
        g_pvF7Why = why;
        return 0;
    }
    g_pvF7Why = PVW_OK;
    InterlockedExchange(&g_pvF7Frame, g_fxFrame + 1);
    g_pvF7Armed = 1;
    return 1;
}
// FxPreviewKeyFrame, every frame of play: 1 = an armed press is due now (its bodies were collected in this frame)
static int PvF7Due(void)
{
    const int when = PvF7When(g_pvF7Armed, (long)g_pvF7Frame, (long)g_fxFrame);
    if (when == PVF7_NONE) return 0;
    g_pvF7Armed = 0;
    if (when == PVF7_NOW) return 1;
    InterlockedExchange(&g_pvF7Frame, -10);
    if (LogQuota(&g_pvQF7, 40))
        Log("screen effects: F7: no picture taken: the game left play before the frame after the press (press F7 again)");
    return 0;
}
// a render target of ours read back (as FxGrabClean: route 1 a system-memory copy; route 2, only after 1 failed, a
// lockable render target) and box-shrunk as the picture (FxPrevShrink). NULL: not available
static DWORD *PvReadBack(void *dev, const ShRt *r, UINT *pow, UINT *poh)
{
    void *volatile mem = NULL;
    FxLockRect8 lr;
    DWORD *volatile out = NULL;
    volatile int route = 0, locked = 0, owned = 0;
    if (!dev || !r->surf || r->w < 64 || r->h < 64) return NULL;
    __try {
        Interp_OwnBegin();  // (a read back, not a frame's draw: not recorded for the interpolated frames)
        owned = 1;
        if (SUCCEEDED(((FxImgSurf_t)VT(dev, 27))(dev, (UINT)r->w, (UINT)r->h, r->fmt, (void **)&mem)) && mem) {
            if (SUCCEEDED(((FxCopyRects_t)VT(dev, 28))(dev, r->surf, NULL, 0, mem, NULL))) route = 1;
            else ((Release_t)VT(mem, VT_RELEASE))(mem), mem = NULL;
        }
        if (!route && SUCCEEDED(((FxRtSurf_t)VT(dev, 25))(dev, (UINT)r->w, (UINT)r->h, r->fmt, 0, TRUE, (void **)&mem)) && mem) {
            if (SUCCEEDED(((FxCopyRects_t)VT(dev, 28))(dev, r->surf, NULL, 0, mem, NULL))) route = 2;
            else ((Release_t)VT(mem, VT_RELEASE))(mem), mem = NULL;
        }
        if (route && SUCCEEDED(((FxSurfLock_t)VT(mem, 9))(mem, &lr, NULL, 0x10 /*READONLY*/)) && lr.bits) locked = 1;
        Interp_OwnEnd();
        owned = 0;
        if (locked) {
            out = FxPrevShrink(lr.bits, lr.pitch, (UINT)r->w, (UINT)r->h, pow, poh);
            ((FxSurfUnlock_t)VT(mem, 10))(mem);
            locked = 0;
        }
        if (mem) ((Release_t)VT(mem, VT_RELEASE))(mem), mem = NULL;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (owned) Interp_OwnEnd();
        __try {
            if (locked && mem) ((FxSurfUnlock_t)VT(mem, 10))(mem);
            if (mem) ((Release_t)VT(mem, VT_RELEASE))(mem);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        if (out) free(out);
        out = NULL;
    }
    return out;
}
static const char *PvBaseName(const char *p)
{
    const char *b = p;
    for (; *p; ++p)
        if (*p == '\\' || *p == '/') b = p + 1;
    return b;
}
// one layer file (temp, then renamed over the old one); 1 = written. *kb its size, *pct the share of lit pixels
static int PvLayWrite(const char *path, const DWORD *px, UINT w, UINT h, int kind, DWORD stamp, unsigned *kb, float *pct)
{
    const size_t cap = PvlMaxBytes(w, h);
    unsigned char *buf = (unsigned char *)malloc(cap);
    char tmp[MAX_PATH];
    PvlU32 lit = 0;
    size_t n;
    HANDLE fh;
    DWORD done = 0;
    int ok;
    *kb = 0, *pct = 0.0f;
    if (!buf) return 0;
    n = PvlEncode((const PvlU32 *)px, w, h, (PvlU32)kind, stamp, buf, cap, &lit);
    ok = n > 0 && sprintf_s(tmp, sizeof tmp, "%s.tmp", path) > 0;
    if (ok) {
        fh = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        ok = fh != INVALID_HANDLE_VALUE;
        if (ok) {
            ok = WriteFile(fh, buf, (DWORD)n, &done, NULL) && done == (DWORD)n;
            CloseHandle(fh);
            ok = ok && MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING);
            if (!ok) DeleteFileA(tmp);
        }
    }
    free(buf);
    if (ok) *kb = (unsigned)((n + 1023) / 1024), *pct = 100.0f * (float)lit / (float)(w * h);
    return ok;
}
// FxPreviewKeyFrame, after FxPrevWrite saved F7's picture (ow x oh, its stamp g_pvStamp): its layers beside it. due 0: the
// press could not arm (or the picture was not F7's frame): no layers, an older file of the names removed
static void PvF7Layers(int due, UINT ow, UINT oh)
{
    void *dev = DEV;
    char path[2][MAX_PATH], note[2][176];
    PvCapIn in;
    PvRun o;
    LARGE_INTEGER t0, t1, qf;
    int k, why = g_pvF7Why, want = 0, have = 0, removed = 0, margin = 0;
    const DWORD stamp = g_pvStamp;
    memset(&o, 0, sizeof o);
    InterlockedExchange(&g_pvF7Frame, -10);  // (this frame's collection is used now)
    if (!PvlPath(g_fxPrevPath, PVLK_HEAT, path[0], MAX_PATH) || !PvlPath(g_fxPrevPath, PVLK_SPIRIT, path[1], MAX_PATH)) return;
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&t0);
    g_pvBodies = g_pvPrints = g_pvGhosts = g_pvFires = g_pvAuras = 0;
    if (due && dev) {
        PvInNow(&in);
        in.f7 = 1;
        want = PvCaptureMask(&in, 1, g_thStamp == g_fxFrame && g_thN > 0, g_gvStamp == g_fxFrame && (g_gvN > 0 || g_svSoulN > 0), &why);
        margin = ShVaMarginMb();
        if (want && !ShKeepTargets(margin)) want = 0, why = -1;  // (the address space is low: no targets made for it)
        if (want) have = PvLayersRun(dev, NULL, 0, 0, want, 1, &o);
        if (want) g_pvSeq = -1, g_pvHave = 0;  // (the targets hold F7's layers now, not a pause's)
    }
    for (k = 0; k < 2; ++k) {
        const int bit = k ? PVL_SPIRIT : PVL_HEAT, kind = k ? PVLK_SPIRIT : PVLK_HEAT;
        int ok = 0;
        note[k][0] = 0;
        if (have & bit) {
            UINT lw = 0, lh = 0;
            unsigned kb = 0;
            float pct = 0.0f;
            DWORD *px = PvReadBack(dev, k ? &g_pvSpirit : &g_pvHeat, &lw, &lh);
            if (!px) sprintf_s(note[k], sizeof note[k], "none (it could not be read back)");
            else if (lw != ow || lh != oh) sprintf_s(note[k], sizeof note[k], "none (%ux%u, not the picture's %ux%u)", lw, lh, ow, oh);
            else if (!(ok = PvLayWrite(path[k], px, lw, lh, kind, stamp, &kb, &pct)))
                sprintf_s(note[k], sizeof note[k], "none (%s could not be written, error %lu)", PvBaseName(path[k]), GetLastError());
            else if (k)
                sprintf_s(note[k], sizeof note[k], "saved %s (%d ghost(s), %d soul fire(s), %d aura sprite(s); %.1f %% of the picture, %u KB)",
                          PvBaseName(path[k]), g_pvGhosts, g_pvFires, g_pvAuras, pct, kb);
            else
                sprintf_s(note[k], sizeof note[k], "saved %s (%d bod%s, %d footprint(s); %.1f %% of the picture, %u KB)", PvBaseName(path[k]),
                          g_pvBodies, g_pvBodies == 1 ? "y" : "ies", g_pvPrints, pct, kb);
            if (px) free(px);
        } else {
            sprintf_s(note[k], sizeof note[k], "none (%s)", why == -1 ? "the address space is low" : !due ? (why > 0 && why < PVW_N ?
                      kPvWhy[why] : "not armed") : want & bit ? "nothing drawn into it" : why > 0 && why < PVW_N ? kPvWhy[why] :
                      k ? "no ghost or soul in that frame" : "nothing hot in that frame");
        }
        if (!ok && DeleteFileA(path[k])) removed |= bit;  // (an older picture's layer: not this picture's any more)
    }
    __try {
        PvRtFree(&g_pvF7Clean);
        if (have && !ShKeepTargets(ShVaMarginMb())) PvFreeAll("F7: the address space is low");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(&g_pvF7Clean, 0, sizeof g_pvF7Clean);
    }
    QueryPerformanceCounter(&t1);
    if (LogQuota(&g_pvQF7, 40))
        Log("screen effects: F7's look layers for %s: heat %s; spirit %s%s; %.1f ms", PvBaseName(g_fxPrevPath), note[0], note[1],
            removed ? "; the older layer file(s) of that name removed" : "",
            qf.QuadPart ? (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)qf.QuadPart : 0.0);
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
// v3.58 PV358: the stock picture's texture (MANAGED, one per device; a missing / unreadable file is tried again at the next
// page opening, logged once a state)
static void *PvStockLoad(void *dev)
{
    int missing = 0;
    void *tex;
    UINT w = 0, h = 0;
    if (g_pvStockDev != dev) g_pvStockDev = dev, g_pvStockTex = NULL, g_pvStockFailed = 0;  // (the old one went with its device)
    if (g_pvStockTex || g_pvStockFailed || !g_pvStockPath[0]) return g_pvStockTex;
    g_pvStockFailed = 1;
    tex = FxDdsLoad(dev, g_pvStockPath, &w, &h, &missing);
    if (!tex) {
        if (missing) g_pvStockHave = 0;
        if (g_pvStockLogged != (missing ? 1 : 2)) {
            g_pvStockLogged = missing ? 1 : 2;
            Log("screen effects: the stock picture %s is %s: Preview %s shows %s instead", g_pvStockPath, missing ? "missing" : "not usable (see above)",
                kPvModeName[g_pvMode], g_pvMode == PVM_LIVE ? "PreviewImage, else SH4 Refined's own scene" : "SH4 Refined's own scene");
        }
        return NULL;
    }
    g_pvStockTex = tex, g_pvStockW = w, g_pvStockH = h, g_pvStockFailed = 0, g_pvStockHave = 1, g_pvStockLogged = 0;
    return tex;
}
// v3.59 PV359: the picture's layer files (F7's set; the stock picture is an F7 picture promoted by the installer)
static void PvLayFree(void)  // the page closed / another layer asked for: the texture released (an older device's: forgotten)
{
    if (g_pvLay.tex && g_pvLay.dev == DEV) {
        __try {
            ((Release_t)VT(g_pvLay.tex, VT_RELEASE))(g_pvLay.tex);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    memset(&g_pvLay, 0, sizeof g_pvLay);
}
// the file at path for the picture at picPath (pw x ph as loaded): checked (pvlayer_sim.h PvlCheck: header, kind, size, the
// picture's stamp, CRC, runs) and decoded into a MANAGED X8R8G8B8 texture. PVLR_*
static int PvLayLoad(void *dev, const char *picPath, const char *path, int kind, UINT pw, UINT ph, void **tex, unsigned *kb)
{
    unsigned char hdr[128], *buf;
    HANDLE f;
    DWORD sz, got = 0;
    PvlU32 stamp = 0, sw = 0, sh = 0;
    int r;
    struct { int pitch; BYTE *bits; } lr;
    *tex = NULL, *kb = 0;
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return PVLR_MISSING;
    sz = GetFileSize(f, NULL);
    if (sz == INVALID_FILE_SIZE || sz < PVL_HDR || (size_t)sz > PvlMaxBytes(PVL_MAX_PIX, 1) || (buf = (unsigned char *)malloc(sz)) == NULL) {
        CloseHandle(f);
        return PVLR_HEADER;  // (no layer of ours is that size)
    }
    if (!ReadFile(f, buf, sz, &got, NULL) || got != sz) got = 0;
    CloseHandle(f);
    *kb = (unsigned)((sz + 1023) / 1024);
    if (!got) {
        free(buf);
        return PVLR_READ;
    }
    f = CreateFileA(picPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);  // the picture's stamp (its header)
    if (f != INVALID_HANDLE_VALUE) {
        DWORD g = 0;
        if (ReadFile(f, hdr, sizeof hdr, &g, NULL)) stamp = PvlPicStamp(hdr, g, &sw, &sh);
        CloseHandle(f);
    }
    r = PvlCheck(buf, sz, (PvlU32)kind, pw, ph, stamp);
    if (r == PVLR_OK && (sw != pw || sh != ph)) r = PVLR_SIZE;  // (the file changed since its texture was made)
    if (r == PVLR_OK) {
        void *t = NULL;
        r = PVLR_TEX;
        if (SUCCEEDED(((SpCreateTex_t)VT(dev, SPV_CREATETEX))(dev, pw, ph, 1, 0, 22 /*X8R8G8B8*/, 1 /*MANAGED*/, &t)) && t) {
            if (SUCCEEDED(((SpLock_t)VT(t, 16))(t, 0, &lr, NULL, 0))) {
                r = PvlDecode(buf, sz, lr.bits, lr.pitch) ? PVLR_OK : PVLR_DATA;
                ((SpUnlock_t)VT(t, 17))(t, 0);
            }
            if (r == PVLR_OK) *tex = t;
            else ((Release_t)VT(t, VT_RELEASE))(t);
        }
    }
    free(buf);
    return r;
}
// the page (PvPageDraw): the drawn picture's layer of this kind (pic 0 the stock picture, 1 PreviewImage), loaded once an
// opening (a failure is not tried again until the page opens again); NULL: none (the filter's look over the picture only)
static void *PvLayGet(void *dev, int pic, int kind)
{
    const char *const picPath = pic ? g_fxPrevPath : g_pvStockPath;
    const UINT pw = pic ? g_fxPrevW : g_pvStockW, ph = pic ? g_fxPrevH : g_pvStockH;
    char path[MAX_PATH];
    void *tex = NULL;
    unsigned kb = 0;
    int r;
    if (g_pvLayOff || (kind != PVLK_HEAT && kind != PVLK_SPIRIT)) return NULL;
    if (g_pvLay.dev == dev && g_pvLay.pic == pic && g_pvLay.kind == kind && (g_pvLay.tex || g_pvLay.tried)) return g_pvLay.tex;
    if (g_pvLay.dev != dev) memset(&g_pvLay, 0, sizeof g_pvLay);  // (an older device's texture went with it)
    else PvLayFree();  // (one layer at a time: the filter's)
    g_pvLay.dev = dev, g_pvLay.pic = pic, g_pvLay.kind = kind, g_pvLay.tried = 1;
    if (!picPath[0] || !pw || !ph || !PvlPath(picPath, kind, path, sizeof path)) return NULL;
    r = PvLayLoad(dev, picPath, path, kind, pw, ph, &tex, &kb);
    if (r == PVLR_OK) g_pvLay.tex = tex, g_pvLay.w = pw, g_pvLay.h = ph;
    if (g_pvLayLast[pic][kind] != r + 1 && LogQuota(&g_pvQLay, 40)) {  // (once a state: each opening reads the file again)
        g_pvLayLast[pic][kind] = r + 1;
        if (r == PVLR_MISSING)
            LogV("screen effects: the picture's %s layer %s: no file (%s shows its look over the picture only)", kPvlKind[kind], path,
                 kind == PVLK_HEAT ? "Thermal" : "Spirit Vision");
        else if (r == PVLR_OK)
            Log("screen effects: the picture's %s layer %s: used (%ux%u, %u KB)", kPvlKind[kind], path, pw, ph, kb);
        else
            Log("screen effects: the picture's %s layer %s: not used: %s (%s shows its look over the picture only)", kPvlKind[kind], path,
                kPvlWhy[r], kind == PVLK_HEAT ? "Thermal" : "Spirit Vision");
    }
    return g_pvLay.tex;
}
// FxBackdropC: what the page shows (PVB_*) and its picture (the pause's, the stock one or PreviewImage); each file is
// loaded only when it decides (preview_sim.h PvBackNeeds*). v3.58 PV358: by Preview (v3.56: PreviewScene, no pause)
static int PvBackPick(void *dev, void **tex, UINT *pw, UINT *ph, int *pause)
{
    PvBackIn in;
    void *pt = NULL, *st = NULL, *it = NULL;
    int back;
    memset(&in, 0, sizeof in);
    *tex = NULL, *pw = *ph = 0, *pause = 0;
    in.mode = g_pvMode;
    in.sceneOk = !g_pvSceneFails;
    if (PvBackNeedsPause(in.mode) && (pt = FxPauseTex(dev)) != NULL) in.pausePic = 1;  // v3.22.0: the paused scene (Live)
    if (PvBackNeedsStock(in.mode, in.pausePic) && (st = PvStockLoad(dev)) != NULL) in.stockOk = 1;
    if (PvBackNeedsPicture(in.mode, in.pausePic, in.stockOk) && (it = FxPrevLoad(dev)) != NULL) in.picture = 1;
    back = PvBackdrop(&in);
    if (back == PVB_PAUSE) *tex = pt, *pw = g_fxPauseTexW, *ph = g_fxPauseTexH, *pause = 1;
    else if (back == PVB_STOCKPIC) *tex = st, *pw = g_pvStockW, *ph = g_pvStockH;
    else if (back == PVB_PICTURE) *tex = it, *pw = g_fxPrevW, *ph = g_fxPrevH;
    return back;
}
// v3.58 PV358: an Effects page (or Shadows) opened with Preview Off: one line an opening of that page
static void PvOffNote(int lp)
{
    static volatile LONG s_q;
    if (lp == g_pvOffNoted || !LogQuota(&s_q, 40)) return;
    g_pvOffNoted = lp;
    Log("screen effects: the Effects page's preview: off (Preview Off) over the game's own menu background (page %d)", lp);
}
// v3.59 PV359 (QA 11; the session: Live's title menu shows the stock picture too): the Shadows page (shadows.inc ShBackdrop /
// ShMode / ShPreviewDraw / ShDarkDesc). The split demonstration only over a live paused game's own shadow (Live); any
// other backdrop is a still picture, shown alone as the Effects page picks it (PvBackPick: Stock the stock picture; Live
// the pause picture, the stock picture, PreviewImage; else SH4 Refined's scene): no split line, no drawn figure.
static int PvShLive(void) { return PvShPage(g_pvMode, 1) == PVSH_SPLIT; }  // the split may show (Live; ShMode's previewOn)
static int PvShStillDrew(void) { return g_pvShDrew == g_fxFrame; }  // this frame's backdrop is the still picture alone: the text panels only
// ShBackdrop (0x526A53, the Shadows page, no paused scene's shadow to split): the still picture alone; 1 = drawn (the
// hallway is skipped)
static int PvShStillBack(void *dev)
{
    void *tex = NULL;
    UINT pw = 0, ph = 0;
    int pause = 0, back = PVB_STOCK, drawn = 0;
    __try {
        back = PvBackPick(dev, &tex, &pw, &ph, &pause);
        if (back == PVB_SCENE) drawn = PvSceneDraw(dev);
        else if (back != PVB_OFF && back != PVB_STOCK && tex && pw && ph) FxDrawPic(dev, tex, pw, ph), drawn = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        drawn = 0;
    }
    if (!drawn) back = PVB_STOCK;
    if ((g_fxFrame - g_pvShSeen > 2 || g_fxFrame < g_pvShSeen || back != g_pvShWhat) && LogQuota(&g_pvQSh, 40))  // (once an opening)
        Log("screen effects: the Shadows page's preview: Preview %s: the still picture alone: %s (the split shadows only over a paused "
            "game, Live)", kPvModeName[(unsigned)g_pvMode < PVM_N ? g_pvMode : PVM_DEFAULT],
            back >= 0 && back < PVB_N ? kPvBack[back] : "?");
    g_pvShWhat = back, g_pvShSeen = g_fxFrame;
    g_pvShDrew = drawn ? g_fxFrame : -1;
    return drawn;
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
    const int key = (int)g_pvFs << 20 | lay << 16 | f << 8 | (g_pvBackFrame == g_fxFrame ? g_pvBack + 1 : 0) << 4 | how;  // v3.58 PV358: + the strength
    char what[160];
    if (key == g_pvNoteKey || !LogQuota(&g_pvQPage, 60)) return;
    g_pvNoteKey = key;
    if (g_pvBackFrame == g_fxFrame && g_pvBack == PVB_OFF) return;  // v3.58 PV358: PvOffNote says it
    if (lay == PVP_PAUSE_HEAT)
        sprintf_s(what, sizeof what, " with the pause's heat layer (%d bod%s, %d footprint(s)%s)", g_pvBodies, g_pvBodies == 1 ? "y" : "ies",
                  g_pvPrints, how == 2 ? "; added: no key shader" : "");
    else if (lay == PVP_PAUSE_SPIRIT)
        sprintf_s(what, sizeof what, " with the pause's spirit layer (%d ghost(s), %d soul fire(s), %d aura sprite(s))", g_pvGhosts, g_pvFires, g_pvAuras);
    else if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT)
        sprintf_s(what, sizeof what, " with %s", kPvPage[lay]);
    else if (lay == PVP_PIC_HEAT || lay == PVP_PIC_SPIRIT) {  // v3.59 PV359: the F7 set's layer file
        char p[MAX_PATH];
        if (!PvlPath(g_pvLay.pic ? g_fxPrevPath : g_pvStockPath, lay == PVP_PIC_HEAT ? PVLK_HEAT : PVLK_SPIRIT, p, sizeof p)) p[0] = 0;
        sprintf_s(what, sizeof what, " with %s (%s)", kPvPage[lay], PvBaseName(p));
    } else
        what[0] = 0;
    Log("screen effects: the Effects page's preview: %s over %s%s%s%s", kFxName[FX_FILTER][f >= 0 && f < FX_FILTERS ? f : 0],
        g_pvBackFrame == g_fxFrame && g_pvBack >= 0 && g_pvBack < PVB_N ? kPvBack[g_pvBack] : "the paused scene (the pause menu)", what,
        how == 0 && lay != PVP_NONE ? " (the layer could not be drawn)" : "",
        f > 0 && g_pvFs != PVS_HIGH ? (g_pvFs == PVS_LOW ? " (Filter Strength Low)" : g_pvFs == PVS_MEDIUM ? " (Filter Strength Medium)" :
                                       " (Filter Strength Ultra)") : "");  // v3.58 PV358
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
    void *lt = NULL;  // v3.59 PV359: the picture's layer
    if (!dev) return;
    memset(&in, 0, sizeof in);
    in.thermal = f == FXF_THERMAL, in.spirit = f == FXF_GHOST;
    in.layersSeq = g_pvSeq, in.picSeq = g_fxPausePicSeq;
    in.haveHeat = (g_pvHave & PVL_HEAT) != 0, in.haveSpirit = (g_pvHave & PVL_SPIRIT) != 0;
    if (where) in.back = ShPauseTex(dev, g_fxPausePicSeq, &pw, &ph) ? PVB_PAUSE : PVB_STOCK;
    else in.back = g_pvBackFrame == g_fxFrame ? g_pvBack : PVB_STOCK;
    if (!where) {  // v3.59 PV359 (QA 3): the stock picture's / PreviewImage's own layer (F7's set), for Thermal / Spirit Vision
        const int k = PvPicLayerWanted(in.back, in.thermal, in.spirit);
        if (k) {
            __try {
                lt = PvLayGet(dev, in.back == PVB_PICTURE, k);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                lt = NULL, g_pvLayOff = 1;
                memset(&g_pvLay, 0, sizeof g_pvLay);
                LogAnomaly("screen effects", "loading the picture's %s layer faulted: the picture layers are off for this session", kPvlKind[k]);
            }
            in.picHeat = lt && k == PVLK_HEAT, in.picSpirit = lt && k == PVLK_SPIRIT;
        }
    }
    lay = PvPageLayer(&in);
    __try {
        if (lay == PVP_PAUSE_HEAT) how = PvLayerDraw(dev, g_pvHeat.tex, (UINT)g_pvHeat.w, (UINT)g_pvHeat.h, 1);
        else if (lay == PVP_PAUSE_SPIRIT) how = PvLayerDraw(dev, g_pvSpirit.tex, (UINT)g_pvSpirit.w, (UINT)g_pvSpirit.h, 0);
        else if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT) how = PvSceneLayer(dev, lay == PVP_SCENE_HEAT);
        else if (lay == PVP_PIC_HEAT || lay == PVP_PIC_SPIRIT) how = PvLayerDraw(dev, lt, g_pvLay.w, g_pvLay.h, lay == PVP_PIC_HEAT);  // v3.59 PV359
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        how = 0;
        if (lay == PVP_SCENE_HEAT || lay == PVP_SCENE_SPIRIT) g_pvSceneFails = 1;
        else if (lay == PVP_PIC_HEAT || lay == PVP_PIC_SPIRIT) g_pvLayOff = 1, PvLayFree();  // v3.59 PV359
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
        if (!shown) PvLayFree();  // v3.59 PV359: the picture's layer goes with the page (read again at the next opening)
        if (shown && !g_pvStockTex && g_pvStockHave <= 0) {  // v3.58 PV358: a file placed since: tried again (an unreadable one is not)
            g_pvStockFailed = 0;
            PvStockCheck();
        }
    }
    if (g_pvOffNoted >= 0 && g_lpage != g_pvOffNoted && g_pending != g_pvOffNoted) g_pvOffNoted = -1;  // v3.58 PV358: a page left: its Off line again next time
    if (g_pvFsRt.tex && !PvFsNeedsCopy((int)g_pvFs, g_fx[FX_FILTER] > 0)) {  // v3.58 PV358: Filter Strength back to High / Ultra (or Filter None): the copy goes
        __try {
            PvRtFree(&g_pvFsRt);
            g_pvFsArmed = -1;
            LogV("screen effects: Filter Strength %s: its plain copy released (the preview's now %.1f MB)", kPvFsName[(int)g_pvFs % PVS_N], ShMb(g_pvVram));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            memset(&g_pvFsRt, 0, sizeof g_pvFsRt);
        }
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

// ================================================================ v3.58 PV358: Filter Strength (the strength's draws)
// FxPass: the look of the filter at Ultra (a copy; High / Low / Medium: the table's own, so High is v3.57's byte for byte)
static int PvFsFam(int f)
{
    if (f == FXF_GHOST) return PVT_GHOST;
    switch (kFxLook[f].tech) {
    case FXT_TONE: return PVT_TONE;
    case FXT_CRT: return PVT_CRT;
    case FXT_VIVID: return PVT_VIVID;
    case FXT_NV: return PVT_NV;
    case FXT_THERMAL: return PVT_THERMAL;
    }
    return PVT_OTHER;
}
static int PvFsLookApply(FxLook *L, int f)
{
    PvFsLookV v;
    if (f <= 0 || f >= FX_FILTERS || g_pvFs != PVS_ULTRA) return 0;
    v.fam = PvFsFam(f);
    v.mix = L->mix, v.con = L->con, v.bri = L->bri, v.px = L->px, v.vig = L->vig, v.scan = L->scan, v.mask = L->mask;
    v.grain = L->grain, v.flicker = L->flicker;
    if (!PvFsLook(&v, PVS_ULTRA)) return 0;
    L->mix = v.mix, L->con = v.con, L->bri = v.bri, L->px = v.px, L->vig = v.vig, L->scan = v.scan, L->mask = v.mask;
    L->grain = v.grain, L->flicker = v.flicker;
    return 1;
}
static void PvFsConstsApply(int f, float c[][4])
{
    const int fam = f > 0 && f < FX_FILTERS ? PvFsFam(f) : PVT_OTHER, v = (int)g_pvFs;
    int r, k;
    if (v != PVS_ULTRA) return;
    for (r = 0; r < FX_NPSC; ++r)
        if (PvFsConstK(fam, v, r))
            for (k = 0; k < 3; ++k) c[r][k] *= kPvFsK[PVS_ULTRA];
}
// FxPass, before the filter's first pass (Low / Medium only): the frame so far into our target (on the GPU, no read back)
static void PvFsGrab(void *dev, int f)
{
    void *rt = NULL;
    FxSurfDesc8 d;
    int made = 0, ok = 0;
    if (!PvFsNeedsCopy((int)g_pvFs, f > 0) || g_pvFsFails >= 3 || !dev) return;
    if (!g_shOn) return;  // (no device-reset hook: a DEFAULT-pool target of ours would fail the game's Reset; the filter shows at High)
    __try {
        PvDevCheck(dev);
        if (SUCCEEDED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) && rt) {
            if (SUCCEEDED(((FxGetDesc_t)VT(rt, 8))(rt, &d)) && (d.fmt == 21 || d.fmt == 22) && d.w >= 64 && d.h >= 64 &&
                PvRtMake(dev, &g_pvFsRt, (int)d.w, (int)d.h, d.fmt, "Filter Strength's plain copy", &made) &&
                SUCCEEDED(((FxCopyRects_t)VT(dev, SHV_COPYRECTS))(dev, rt, NULL, 0, g_pvFsRt.surf, NULL)))
                ok = 1;
            ((Release_t)VT(rt, VT_RELEASE))(rt);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = 0;
    }
    g_pvFsArmed = ok ? g_fxFrame : -1;
    if (!ok && ++g_pvFsFails && LogQuota(&g_pvFsQ, 4))
        Log("screen effects: Filter Strength %s: the plain picture could not be copied (%d of 3%s): this frame shows the filter at High",
            kPvFsName[(int)g_pvFs % PVS_N], g_pvFsFails, g_pvFsFails >= 3 ? "; High for the rest of the session" : "");
}
// FxPass after its overlays / GvPass after its edge world (before the hot bodies, the ghosts and the fires): the plain
// picture of this frame back over the filter at 1 - k
static void PvFsMix(void *dev)
{
    FxSaved sv;
    const float a = PvFsMixAlpha((int)g_pvFs);
    if (g_pvFsArmed != g_fxFrame || !dev) return;
    g_pvFsArmed = -1;  // (once: a second pass in this frame takes its own copy)
    if (!(a > 0.0f) || !g_pvFsRt.tex || !FxSave(dev, &sv, 0)) return;
    __try {
        FxBase(dev);
        ((SpSetDW_t)VT(dev, SPV_SETPS))(dev, 0);
        ((SpSetDW_t)VT(dev, SPV_SETVS))(dev, 0x144);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 1 /*COLOROP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 2 /*COLORARG1*/, 2 /*TEXTURE*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 4 /*ALPHAOP*/, 2 /*SELECTARG1*/);
        ((SpSetTSS_t)VT(dev, SPV_SETTSS))(dev, 0, 5 /*ALPHAARG1*/, 0 /*DIFFUSE*/);
        FxSampler(dev, g_pvFsRt.tex, 0, 1);  // (point: the copy is the frame's own size)
        PvBlend(dev, 5 /*SRCALPHA*/, 6 /*INVSRCALPHA*/);
        FxRect(dev, (DWORD)(a * 255.0f + 0.5f) << 24 | 0x00FFFFFFu, 0, 0, FxW(), FxH(), 0, 0, 1, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (++g_pvFsFails >= 3 && LogQuota(&g_pvFsQ, 4))
            LogAnomaly("screen effects", "Filter Strength's mix faulted: High for the rest of the session");
    }
    FxRestore(dev, &sv);
}

// ================================================================ v3.58 PV358: the two rows (Effects > Filter Strength, Preview)
// Origin rows of the Screen Effects page (sh4windowmod.c, after the DoF rows); layout.inc places them on Effects (OLS_FSTR /
// OLS_PREVIEW, by their inputs). Texts MSG_V58 + 16 .. 31 (PV358's ids); the value lines come from the plan (vlines.inc
// answers them while the getter answers the row's line); Filter Strength is grey with Filter None (look.inc LKR_FILTER).
enum { PVX_FS_L, PVX_FS_D, PVX_FS_V0, PVX_PV_L = PVX_FS_V0 + PVS_N, PVX_PV_D, PVX_PV_V0, PVX_PV_NOFILE = PVX_PV_V0 + PVM_N,
       PVX_FILTER, PVX_N };
typedef char PvTextsFit[PVX_N <= 16 ? 1 : -1];  // PV358's V58 + 16 .. 31
static BYTE g_pvTxt[PVX_N][128];
static DWORD g_pvFsIds[PVS_N], g_pvModeIds[PVM_N];
static int g_pvFilterF = -1;  // the filter whose line + F7 sentence PVX_FILTER holds
static const BYTE *PvText(int id)
{
    const int i = id - (MSG_V58 + 16);
    static const BYTE kEmpty[4] = { 0x00, 0x80, 0xFF, 0xFF };
    return i >= 0 && i < PVX_N && g_pvTxt[i][1] == 0x80 ? g_pvTxt[i] : kEmpty;
}
static void PvTexts(void)
{
    static const char *const kFs[PVS_N] = { OLW_EFFECTS_FILTER_STRENGTH_V_LOW, OLW_EFFECTS_FILTER_STRENGTH_V_MEDIUM,
                                            OLW_EFFECTS_FILTER_STRENGTH_V_HIGH, OLW_EFFECTS_FILTER_STRENGTH_V_ULTRA };
    static const char *const kMode[PVM_N] = { OLW_EFFECTS_PREVIEW_V_OFF, OLW_EFFECTS_PREVIEW_V_STOCK, OLW_EFFECTS_PREVIEW_V_LIVE };
    typedef char PvWordsAreIni[PVS_N == 4 && PVM_N == 3 ? 1 : -1];  // (the screen words are the ini words: preview_sim_test checks)
    int v;
    Encode(g_pvTxt[PVX_FS_L], 128, OLW_EFFECTS_FILTER_STRENGTH_L);
    Encode(g_pvTxt[PVX_FS_D], 128, OLW_EFFECTS_FILTER_STRENGTH_D);
    Encode(g_pvTxt[PVX_PV_L], 128, OLW_EFFECTS_PREVIEW_L);
    Encode(g_pvTxt[PVX_PV_D], 128, OLW_EFFECTS_PREVIEW_D);
    Encode(g_pvTxt[PVX_PV_NOFILE], 128, OLW_PV_STOCK_NOFILE);
    for (v = 0; v < PVS_N; ++v) Encode(g_pvTxt[PVX_FS_V0 + v], 128, kFs[v]), g_pvFsIds[v] = MSG_V58 + 16 + PVX_FS_V0 + v;
    for (v = 0; v < PVM_N; ++v) Encode(g_pvTxt[PVX_PV_V0 + v], 128, kMode[v]), g_pvModeIds[v] = MSG_V58 + 16 + PVX_PV_V0 + v;
    g_pvFilterF = -1;
}
static void PvFsSet(int v, int save)
{
    if (v < 0 || v >= PVS_N) return;
    InterlockedExchange(&g_pvFs, v);
    if (save && !g_iniFallback) {  // the value, then the marker (a lost marker reads High at the next start: never a v3.15 value)
        static volatile LONG s_q;
        const char *fail = NULL;
        if (!PvIniWrite("FilterStrength", kPvFsName[v])) fail = "the value";
        else if (!g_pvFsMarked && !PvIniWrite("FilterStrengthV", PV_FS_MARK)) fail = "the marker FilterStrengthV (the next start reads High)";
        else g_pvFsMarked = 1;
        if (fail && LogQuota(&s_q, 5))
            Log("screen effects: [ScreenEffects] FilterStrength=%s: %s could not be written to SH4Refined.ini (error %lu): this session only",
                kPvFsName[v], fail, GetLastError());
    }
    LogV("screen effects: Filter Strength %s (Filter %s)", kPvFsName[v], kFxName[FX_FILTER][(unsigned)g_fx[FX_FILTER] < FX_FILTERS ? (int)g_fx[FX_FILTER] : 0]);
}
static void PvModeSet(int v, int save)
{
    if (v < 0 || v >= PVM_N) return;
    g_pvMode = v;
    if (save && !g_iniFallback && !PvIniWrite("Preview", kPvModeName[v])) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5))
            Log("screen effects: [ScreenEffects] Preview=%s could not be written to SH4Refined.ini (error %lu): this session only", kPvModeName[v],
                GetLastError());
    }
    g_pvNoteKey = -1, g_pvOffNoted = -1;  // the page logs what it shows now
    LogV("screen effects: Preview %s (the stock picture %s)", kPvModeName[v], g_pvStockHave > 0 ? "present" : "missing");
}
static int __cdecl PvFsGet(void) { return (unsigned)g_pvFs < PVS_N ? (int)g_pvFs : PVS_DEFAULT; }
static int __cdecl PvModeGet(void) { return (unsigned)g_pvMode < PVM_N ? g_pvMode : PVM_DEFAULT; }
static int __cdecl PvFsIn(BYTE *cfg, int count)
{
    const int v = StepValue(PvFsGet(), count);
    (void)cfg;
    if (v >= 0) PvFsSet(v, 1);
    return 0;
}
static int __cdecl PvModeIn(BYTE *cfg, int count)
{
    const int v = StepValue(PvModeGet(), count);
    (void)cfg;
    if (v >= 0) PvModeSet(v, 1);
    return 0;
}
static int __cdecl PvFsDesc(void) { return MSG_V58 + 16 + PVX_FS_D; }  // the row's line (vlines.inc: each value's own)
static int __cdecl PvModeDesc(void)  // Stock without the file says so; else the row's line (vlines.inc: each value's own)
{
    return MSG_V58 + 16 + (g_pvMode == PVM_STOCK && g_pvStockHave == 0 ? PVX_PV_NOFILE : PVX_PV_D);
}
static int PvBuildRows(DWORD *rows, int row0)
{
    PvTexts();
    FillRow(rows, PvFsIn, PvFsGet, g_pvFsIds, PVS_N, MSG_V58 + 16 + PVX_FS_L, PvFsDesc);
    FillRow(rows + 7, PvModeIn, PvModeGet, g_pvModeIds, PVM_N, MSG_V58 + 16 + PVX_PV_L, PvModeDesc);
    g_pvRows = 1, g_pvRow0 = row0;
    LogV("screen effects: Filter Strength and Preview are rows %d and %d of the Screen Effects page", row0, row0 + 1);
    return 2;
}
static void PvSnapshot(void) { g_snapPvMode = g_pvMode, g_snapPvFs = g_pvFs; }
static int PvChanged(void) { return g_snapOn && g_pvRows && (g_pvMode != g_snapPvMode || g_pvFs != g_snapPvFs); }
static int PvFxRowChanged(int row)  // -1 = not one of ours
{
    if (!g_pvRows || g_pvRow0 < 0 || row < g_pvRow0 || row > g_pvRow0 + 1) return -1;
    if (!g_snapOn) return 0;
    return row == g_pvRow0 ? g_pvFs != g_snapPvFs : g_pvMode != g_snapPvMode;
}
static void PvDiscard(void)  // "No" at the exit prompt: both back as Options opened (written back)
{
    if (!g_snapOn) return;
    if (g_pvFs != g_snapPvFs) PvFsSet((int)g_snapPvFs, 1);
    if (g_pvMode != g_snapPvMode) PvModeSet((int)g_snapPvMode, 1);
}
// FxDesc3 (the Filter row): its value line + " Press F7 in play for a new preview." only where F7 changes what the page
// shows (Live, outside a pause, no stock picture: PvF7Shows); 0 = the usual line (sh4windowmod.c's, no sentence)
static int PvFilterDesc(int f)
{
    static const char *const kLine[OL_FILTERS] = {  // (sh4windowmod.c's kFilterLine: the plan's value lines)
        OLW_EFFECTS_FILTER_VD_NONE, OLW_EFFECTS_FILTER_VD_CRT, OLW_EFFECTS_FILTER_VD_VIVID, OLW_EFFECTS_FILTER_VD_SEPIA,
        OLW_EFFECTS_FILTER_VD_OLD_FILM, OLW_EFFECTS_FILTER_VD_NIGHT_VISION, OLW_EFFECTS_FILTER_VD_NIGHT_MONO,
        OLW_EFFECTS_FILTER_VD_THERMAL_VISION, OLW_EFFECTS_FILTER_VD_SPIRIT_VISION, OLW_EFFECTS_FILTER_VD_VIGNETTE };  // v3.59 TV359: Thermal Vision
    const int back = g_fxFrame - g_pvBackFrame <= 2 && g_fxFrame >= g_pvBackFrame ? g_pvBack : -1;
    char s[128], k[48];
    if ((unsigned)f >= OL_FILTERS || g_fxKeyVk < VK_F1 || g_fxKeyVk > VK_F12 || !PvF7Shows(g_pvMode, back)) return 0;
    if (f != g_pvFilterF) {
        sprintf_s(k, sizeof k, OLW_FILTER_F7, g_fxKeyVk - VK_F1 + 1);
        sprintf_s(s, sizeof s, "%s%s", kLine[f], k);
        Encode(g_pvTxt[PVX_FILTER], 128, s);
        g_pvFilterF = f;
    }
    return MSG_V58 + 16 + PVX_FILTER;
}
