// ================================================================ v3.37 CP337: the performance overlay and the backend line
// USER_FEEDBACK_v3361 item 12 (the user's choice: "Performance overlay (row), Backend line (Display Mode description)";
// RESEARCH.md section 6: dgVoodoo2 2.87.6 -> D3D12 only changes the presentation, Refined still draws through D3D8 ->
// GOG's wrapper -> D3D9, so the useful extras are a readout and naming the chain). Plan: staging\v337_feedback\
// w3_camera_perf\PLAN.md. Pure code: perfhud_sim.h (test\perfhud_sim_test.c). Included last by sh4windowmod.c (after
// loghealth.inc: it reads the ledger total g_lhOwnMs; screenfx.inc's FxSave / FxBase / FxRestore / FxOnBackBuffer and
// items_draw.inc's SpVtx / vtable indices draw it). No exe patch of its own: it runs in two existing hooks, game thread:
//   * the frame end (0x415202, FrameEndC, every frame of every mode: boot, title, Options, play, pause, cut-scenes)
//     measures: the frame's length (QueryPerformanceCounter between frame ends), a ring of the last 1024 for the 1 % low
//     of the latest 5 s, the half second's average fps / frame ms and Refined's per-frame work (the budget ledger's frame
//     total, loghealth.inc LhFrameEndC: frames with the HUD call only), the address space in use (GlobalMemoryStatusEx,
//     once a second; 2048 MB: the exe is not large-address aware);
//   * right after the stock post pass (0x55AC29, screenfx.inc FxAfterPost; the brightness / fade pass and the pause
//     picture's copy have both run: the readout never lands in the frozen picture and is never dimmed by a fade) draws
//     four lines in the top-left corner as untextured quads (a 5 x 7 pixel font, 2 / 3 px a pixel at 1080 / 1440) over a
//     dark box, every device state saved and put back (FxSave / FxRestore). Without the screen-effects hook it draws at
//     the frame end instead (skipping the frame the pause picture is copied from), without the frame-rate hook it
//     measures at the post pass.
// The numbers change twice a second. It never feeds the effect budget's governor (BgFrame) nor the frame-rate governor.
// [Graphics] PerfOverlay (0 / 1): v3.37 an Options > Option 4 > Performance row; v3.39 PR339 (USER_FEEDBACK_v3381 item 4,
// the user's "hidden key + log"): no row, a hidden key read at start (1 = the readout). Log: a summary every 5 minutes
// whether the readout shows or not (LogV: the frames are measured either way), the Direct3D chain once (Log; v3.39 PR339:
// with the live Graphics Plus variant's name, presentation.inc PresBootVariant), a clock that runs backwards or a fault
// (LogAnomaly, rate-limited).
// v3.38 HB338 (USER_FEEDBACK_v337 item 17: at 4:3 it sat on Playable Eileen's health HUD): the readout keeps the corner
// unless it would meet the health HUD's box (Playable Eileen's [HUD] place, else the stock gauge's); then it goes under
// it (else right of it). Decided once per size / squeeze / PE ini change (PerfPlaceCheck), logged (LogV "perf: readout").
#include "perfhud_sim.h"

enum { PERFT_LABEL, PERFT_DESC, PERFT_DISPD, PERFT_ASPD, PERFT_N };  // v3.39 PR339: LABEL / DESC unused (no row; kept, not renumbered)
typedef char PerfTextsFit[PERFT_N <= MSG_INV0 - MSG_PERF0 ? 1 : -1];  // v3.37 MERGE: the block ends at MSG_INV0
static BYTE g_txtPerf[PERFT_N][112];
static volatile LONG g_perfOn;            // [Graphics] PerfOverlay (v3.39 PR339: hidden, read at start)
static int g_perfFail;                    // a fault: off for the session
static int g_perfRun;                     // measuring (the first frame after it went on starts the clocks)
static int g_perfHave;                    // the lines hold numbers
static LARGE_INTEGER g_perfQf, g_perfPrev, g_perfWinT0, g_perfSumT0;
static PerfRing g_perfRing;
static PerfWin g_perfWin;
static char g_perfLine[PERF_LINES][PERF_LINE_MAX];
static unsigned g_perfRgb[PERF_LINES];
static LONG g_perfOwnSeen;                // loghealth.inc's g_lhOwnN last taken
static int g_perfVaUsed, g_perfVaTotal, g_perfVaMax;
static DWORD g_perfVaTick;
static struct { unsigned frames, own, shown; double ms, worst, ownMs; float lowMin; } g_perfSum;  // the summary's period
static int g_perfBackDone;
static char g_perfBackend[96];

static float PerfMs(LONGLONG d) { return g_perfQf.QuadPart ? (float)((double)d * 1000.0 / (double)g_perfQf.QuadPart) : 0.0f; }

// ---------------------------------------------------------------- texts (MSG_PERF0..), the backend line
static const BYTE *PerfText(int id)
{
    const int i = id - MSG_PERF0;
    return g_txtPerf[i >= 0 && i < PERFT_N ? i : PERFT_LABEL];
}
// The Direct3D chain, once the game's d3d9 is loaded (Options run long after the device is made): the d3d9.dll in use
// (the game folder's = dgVoodoo2 when dgVoodoo.conf is there, as loghealth.inc's session line), its [General] OutputAPI,
// and the system d3d12 / d3d11 it loaded. Appended to Display Mode's description (Aspect Ratio's without that row).
static void PerfBackendDetect(void)
{
    const HMODULE d9 = GetModuleHandleA("d3d9.dll");
    char path[MAX_PATH], conf[MAX_PATH], api[64], s[160];
    int folder = 0, haveConf;
    if (g_perfBackDone || !d9) return;
    g_perfBackDone = 1;
    if (GetModuleFileNameA(d9, path, MAX_PATH)) {
        char *sl = strrchr(path, '\\');
        if (sl) {
            sl[1] = 0;
            folder = !_stricmp(path, g_dir);
        }
    }
    sprintf_s(conf, sizeof conf, "%sdgVoodoo.conf", g_dir);
    haveConf = GetFileAttributesA(conf) != INVALID_FILE_ATTRIBUTES;
    api[0] = 0;
    if (haveConf) GetPrivateProfileStringA("General", "OutputAPI", "", api, sizeof api, conf);
    PerfBackendName(folder, haveConf, api, GetModuleHandleA("d3d12.dll") != NULL, GetModuleHandleA("d3d11.dll") != NULL,
                    folder && haveConf ? PresBootVariant() : NULL, g_perfBackend, sizeof g_perfBackend);  // v3.39 PR339: + the variant
    sprintf_s(s, sizeof s, "A window, or the whole screen. %s.", g_perfBackend);
    Encode(g_txtPerf[PERFT_DISPD], sizeof g_txtPerf[0], s);
    sprintf_s(s, sizeof s, "Sets the shape of the picture. %s.", g_perfBackend);
    Encode(g_txtPerf[PERFT_ASPD], sizeof g_txtPerf[0], s);
    Log("perf: %s (Option 4's first row description; d3d9.dll from %s, dgVoodoo.conf %s, OutputAPI \"%s\", d3d12.dll %s, "
        "d3d11.dll %s)", g_perfBackend, folder ? "the game folder" : "the system", haveConf ? "present" : "missing", api,
        GetModuleHandleA("d3d12.dll") ? "loaded" : "not loaded", GetModuleHandleA("d3d11.dll") ? "loaded" : "not loaded");
}
// Display Mode's (which 0) / Aspect Ratio's (which 1) description id: with the chain once it is known
// (v3.53 OL1: no longer asked: the Renderer row's line names the backend, PerfBackendLine)
static int PerfDescId(int stockId, int which)
{
    PerfBackendDetect();
    return g_perfBackDone ? MSG_PERF0 + (which ? PERFT_ASPD : PERFT_DISPD) : stockId;
}
// v3.53 OL1: the backend text for the Renderer row (presentation.inc RendDesc; "" until d3d9.dll is loaded)
static const char *PerfBackendLine(void)
{
    PerfBackendDetect();
    return g_perfBackDone ? g_perfBackend : "";
}

// (v3.39 PR339: the Options row Option 4 > Performance is gone; the readout is the hidden key's)
// Init (before the Options tables): the hidden key and the texts
static void PerfInstall(void)
{
    g_perfOn = GetPrivateProfileIntA("Graphics", "PerfOverlay", 0, g_iniPath) != 0;  // v3.39 PR339: hidden (was IniInt + the row)
    Encode(g_txtPerf[PERFT_LABEL], sizeof g_txtPerf[0], "Performance");
    Encode(g_txtPerf[PERFT_DESC], sizeof g_txtPerf[0], "A corner readout: fps, frame time, 1% low, Refined's ms, memory.");
    Encode(g_txtPerf[PERFT_DISPD], sizeof g_txtPerf[0], "A window, or the whole screen.");
    Encode(g_txtPerf[PERFT_ASPD], sizeof g_txtPerf[0], "Sets the shape of the picture.");
    Log("perf: overlay %s (hidden [Graphics] PerfOverlay=1 shows it): fps, frame ms, 1%% low of the last %.0f s, Refined's ms, "
        "address space; measured at the frame end%s%s; a summary in the log every 5 minutes", g_perfOn ? "on" : "off",
        PERF_LOW_MS / 1000.0f, g_rateHooks ? "" : " (no frame-rate hook: at the post pass)",
        !g_perfOn ? "" : g_fxOn ? ", drawn after the post pass" : ", drawn at the frame end (no screen-effects hook)");
}

// ---------------------------------------------------------------- measuring (game thread, once a frame)
static void PerfVaPoll(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    if (!GlobalMemoryStatusEx(&ms)) return;
    g_perfVaTotal = (int)(ms.ullTotalVirtual >> 20);
    g_perfVaUsed = (int)((ms.ullTotalVirtual - ms.ullAvailVirtual) >> 20);
    if (g_perfVaUsed > g_perfVaMax) g_perfVaMax = g_perfVaUsed;
}
static void PerfSumReset(LARGE_INTEGER now)
{
    memset(&g_perfSum, 0, sizeof g_perfSum);
    g_perfSum.lowMin = 0.0f;
    g_perfSumT0 = now;
    g_perfVaMax = g_perfVaUsed;
}
// LogV: the period's numbers (every 5 minutes while on, and when it goes off): what a tester reads next to the room lines
static void PerfSummary(const char *why)
{
    static LONG s_q;
    LARGE_INTEGER now;
    float p99 = 0.0f, low;
    int used = 0;
    QueryPerformanceCounter(&now);
    low = PerfLowFps(&g_perfRing, PERF_LOW_MS, &p99, &used);
    if (g_perfSum.frames && LogQuota(&s_q, 400))
        LogV("perf: %s: %.0f s, %u frames, %.1f fps on average, the lowest 1%% low %.1f fps (now %.1f, p99 %.1f ms), worst frame "
             "%.1f ms, Refined %.2f ms a frame (%u ledger frames), address space %d of %d MB (most %d)", why,
             PerfMs(now.QuadPart - g_perfSumT0.QuadPart) / 1000.0f, g_perfSum.frames,
             g_perfSum.ms > 0.0 ? 1000.0 * g_perfSum.frames / g_perfSum.ms : 0.0, g_perfSum.lowMin, low, p99, g_perfSum.worst,
             g_perfSum.own ? g_perfSum.ownMs / g_perfSum.own : 0.0, g_perfSum.own, g_perfVaUsed, g_perfVaTotal, g_perfVaMax);
    PerfSumReset(now);
}
static void PerfMeasure(void)
{
    static LONG s_qa;
    LARGE_INTEGER now;
    const DWORD tick = GetTickCount();
    float dt;
    QueryPerformanceCounter(&now);
    if (!g_perfQf.QuadPart) QueryPerformanceFrequency(&g_perfQf);
    if (!g_perfRun) {  // the first frame after it went on: the clocks start
        g_perfRun = 1;
        g_perfHave = 0;
        g_perfPrev = g_perfWinT0 = now;
        PerfRingReset(&g_perfRing);
        PerfWinReset(&g_perfWin);
        g_perfOwnSeen = g_lhOwnN;
        PerfVaPoll();
        g_perfVaTick = tick;
        PerfSumReset(now);
        return;
    }
    dt = PerfMs(now.QuadPart - g_perfPrev.QuadPart);
    g_perfPrev = now;
    if (PerfRingPush(&g_perfRing, dt)) {
        PerfWinFrame(&g_perfWin, dt);
        ++g_perfSum.frames;
        g_perfSum.ms += dt;
        if (dt > g_perfSum.worst) g_perfSum.worst = dt;
    } else if (!(dt > 0.0f) && LogQuota(&s_qa, 5)) {
        LogAnomaly("perf", "the frame clock went backwards or stood still (%.3f ms between two frame ends): that frame is not counted", dt);
    }
    if (g_lhOwnN != g_perfOwnSeen) {  // a frame with the ledger (the HUD call ran: worlds, Room 302, the pause)
        g_perfOwnSeen = g_lhOwnN;
        PerfWinOwn(&g_perfWin, g_lhOwnMs);
        ++g_perfSum.own;
        g_perfSum.ownMs += g_lhOwnMs;
    }
    if (tick - g_perfVaTick >= 1000) {
        g_perfVaTick = tick;
        PerfVaPoll();
    }
    if (PerfMs(now.QuadPart - g_perfWinT0.QuadPart) >= PERF_SHOW_MS) {
        PerfShown sh;
        float p99;
        int used;
        const int rate = *(volatile int *)0x5D5248;
        const float low = PerfLowFps(&g_perfRing, PERF_LOW_MS, &p99, &used);
        PerfShownFrom(&g_perfWin, low, g_perfVaUsed, g_perfVaTotal, rate, &sh);
        PerfLines(&sh, g_perfLine, g_perfRgb);
        g_perfHave = 1;
        if (low > 0.0f && (g_perfSum.lowMin <= 0.0f || low < g_perfSum.lowMin)) g_perfSum.lowMin = low;
        PerfWinReset(&g_perfWin);
        g_perfWinT0 = now;
        if ((++g_perfSum.shown & 7) == 0)  // trace (Level 3): every 4 s, about 0.1 MB an hour
            LogT("perf: %s | %s | %s | %s", g_perfLine[0], g_perfLine[1], g_perfLine[2], g_perfLine[3]);
    }
    if (PerfMs(now.QuadPart - g_perfSumT0.QuadPart) >= 300000.0f) PerfSummary("the last 5 minutes");
}

// ---------------------------------------------------------------- drawing
#define PERF_MAXR 1024
static PerfRect g_perfR[PERF_MAXR];
static SpVtx g_perfV[(PERF_MAXR + 1) * 6];
static void PerfQuad(int *nv, float x, float y, float w, float h, DWORD argb)
{
    SpVtx q[4];
    int m;
    if (*nv + 6 > (int)(sizeof g_perfV / sizeof g_perfV[0])) return;
    for (m = 0; m < 4; ++m) {
        q[m].x = x + ((m & 1) ? w : 0.0f) - 0.5f;
        q[m].y = y + ((m & 2) ? h : 0.0f) - 0.5f;
        q[m].z = 0.0f;
        q[m].rhw = 1.0f;
        q[m].c = argb;
        q[m].u = q[m].v = 0.0f;
    }
    g_perfV[(*nv)++] = q[0], g_perfV[(*nv)++] = q[1], g_perfV[(*nv)++] = q[2];
    g_perfV[(*nv)++] = q[1], g_perfV[(*nv)++] = q[3], g_perfV[(*nv)++] = q[2];
}
// ---------------------------------------------------------------- v3.38 HB338: the readout's place (perfhud_sim.h PerfPlace)
// Never over the health HUD: Playable Eileen's ([HUD] X / Y / Scale / Aspect in SH4PlayableEileen.ini, while dsound.dll
// with SH4PE_Patches is loaded and its Style is not Original) or the stock gauge. Decided again only when the back buffer's
// size, the 2D squeeze S, Playable Eileen's presence or its ini's write time changes (those two looked at every 2 s), so
// the readout never jumps between frames.
static struct { int ok, W, H, pe, hud, where; float S, bx, by; FILETIME ini; } g_perfPl;
static DWORD g_perfPlTick;
static int g_perfPlPe;
static FILETIME g_perfPlIni;
static float PerfIniF(const char *key, float def)
{
    char b[32], *e;
    double v;
    GetPrivateProfileStringA("HUD", key, "", b, sizeof b, g_peIni);
    v = strtod(b, &e);
    return e != b && v == v ? (float)v : def;
}
static void PerfPlaceCheck(int W, int H)
{
    const DWORD now = GetTickCount();
    const float S = SH4WM_Scale2D();
    PerfBox box;
    char style[32], asp[32];
    float X = 24.0f, Y = 24.0f, sc = 1.0f, aspect = 0.0f;
    int hud = 0;
    if (!g_perfPl.ok || now - g_perfPlTick >= 2000) {
        const HMODULE ds = GetModuleHandleA("dsound.dll");
        WIN32_FILE_ATTRIBUTE_DATA a;
        g_perfPlTick = now;
        g_perfPlPe = ds && HasExport(ds, "SH4PE_Patches");
        memset(&g_perfPlIni, 0, sizeof g_perfPlIni);
        if (g_perfPlPe && GetFileAttributesExA(g_peIni, GetFileExInfoStandard, &a)) g_perfPlIni = a.ftLastWriteTime;
    }
    if (g_perfPl.ok && g_perfPl.W == W && g_perfPl.H == H && g_perfPl.S == S && g_perfPl.pe == g_perfPlPe &&
        !CompareFileTime(&g_perfPl.ini, &g_perfPlIni))
        return;
    if (g_perfPlPe) {  // its [HUD] keys as PE reads them (Style New = Glass, Stock = Original; Aspect Auto / Stock / a number)
        GetPrivateProfileStringA("HUD", "Style", "Glass", style, sizeof style, g_peIni);
        hud = _stricmp(style, "Original") && _stricmp(style, "Stock");
        X = PerfIniF("X", 24.0f), Y = PerfIniF("Y", 24.0f), sc = PerfIniF("Scale", 1.0f);
        GetPrivateProfileStringA("HUD", "Aspect", "Auto", asp, sizeof asp, g_peIni);
        aspect = !_strnicmp(asp, "Stock", 5) ? 0.892857f : (float)atof(asp);
    }
    box = hud ? PerfPeHudBox((float)W, (float)H, S, X, Y, sc, aspect) : PerfStockGaugeBox((float)W, (float)H, S);
    g_perfPl.where = PerfPlace((float)W, (float)H, &box, &g_perfPl.bx, &g_perfPl.by);
    g_perfPl.ok = 1, g_perfPl.W = W, g_perfPl.H = H, g_perfPl.S = S, g_perfPl.pe = g_perfPlPe, g_perfPl.hud = hud;
    g_perfPl.ini = g_perfPlIni;
    {
        static const char *const kAt[4] = { "at the top-left corner, clear of", "under", "right of", "at the top-left corner, over" };
        static LONG s_q, s_qa;
        char what[128];
        if (hud) sprintf_s(what, sizeof what, "Playable Eileen's HUD ([HUD] X %.0f, Y %.0f, Scale %.2f)", X, Y, sc);
        else sprintf_s(what, sizeof what, "the stock health gauge%s", g_perfPlPe ? " (Playable Eileen's [HUD] Style Original)" : "");
        if (LogQuota(&s_q, 40))
            LogV("perf: readout %s %s (%dx%d, S %.3f; the HUD box %.0f..%.0f x %.0f..%.0f px): its box from %.0f, %.0f px",
                 kAt[g_perfPl.where], what, W, H, S, box.x0, box.x1, box.y0, box.y1, g_perfPl.bx, g_perfPl.by);
        if (g_perfPl.where == PERF_AT_OVER && LogQuota(&s_qa, 3))
            LogAnomaly("perf", "the readout fits neither under nor right of %s at %dx%d: it stays in the corner over it", what, W, H);
    }
}
static void PerfDrawC(int where)
{
    static int s_logged;
    static LONG s_qd;
    void *const dev = DEV;
    const float W = (float)*(volatile int *)0x5D524C, H = (float)*(volatile int *)0x5D5250;
    FxSaved sv;
    float u, x0, y0;
    int l, i, nv = 0, wmax = 0, dropped = 0;
    if (!g_perfHave || !dev || !(W >= 64.0f) || !(H >= 64.0f) || W > 16384.0f || H > 16384.0f) return;
    if (where == 0 && *(volatile int *)0x12E3E2C == 1) return;  // (the frame-end fallback) the pause picture is copied now
    u = (float)PerfScale((int)H);
    PerfPlaceCheck((int)W, (int)H);  // v3.38 HB338: the corner, or under / right of the health HUD
    x0 = g_perfPl.bx + 3.0f * u;     // (the text starts 3 font pixels inside its box; v3.37: always 6u, 6u)
    y0 = g_perfPl.by + 3.0f * u;
    for (l = 0; l < PERF_LINES; ++l)
        if (PerfTextWidth(g_perfLine[l]) > wmax) wmax = PerfTextWidth(g_perfLine[l]);
    PerfQuad(&nv, x0 - 3.0f * u, y0 - 3.0f * u, (float)wmax * u + 6.0f * u, (float)(PERF_LINES * PERF_LINE_H - 2) * u + 6.0f * u,
             0x8C000000u);  // the dark box (55 %)
    for (l = 0; l < PERF_LINES; ++l) {
        const int n = PerfTextRects(g_perfLine[l], x0, y0 + (float)(l * PERF_LINE_H) * u, u, g_perfR, PERF_MAXR, &dropped);
        const DWORD c = 0xFF000000u | (g_perfRgb[l] & 0xFFFFFFu);
        for (i = 0; i < n; ++i) PerfQuad(&nv, g_perfR[i].x, g_perfR[i].y, g_perfR[i].w, g_perfR[i].h, c);
    }
    if (dropped && LogQuota(&s_qd, 3)) LogAnomaly("perf", "%d rectangle(s) of the readout did not fit the buffer (%d)", dropped, PERF_MAXR);
    if (!FxOnBackBuffer(dev) || !FxSave(dev, &sv, 0)) return;
    __try {
        FxBase(dev);
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
        ((SpDrawUP_t)VT(dev, SPV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(nv / 3), g_perfV, sizeof(SpVtx));
    } __finally {
        FxRestore(dev, &sv);
    }
    if (!s_logged++)
        LogV("perf: first readout drawn (%s; target %.0fx%.0f, %.0f px a font pixel, %d triangles): %s | %s | %s | %s",
             where ? "after the post pass" : "at the frame end", W, H, u, nv / 3, g_perfLine[0], g_perfLine[1], g_perfLine[2],
             g_perfLine[3]);
}

// where 0 = the frame end (FrameEndC, before 0x55ABF0), 1 = after the post pass (FxAfterPost, 0x55AC29)
// v3.39 PR339: the frames are measured whether the readout shows or not (the 5-minute summaries stay in the log)
static void PerfFrame(int where)
{
    const int measureAt = g_rateHooks ? 0 : 1, drawAt = g_fxOn ? 1 : 0;
    if (g_perfFail) {
        g_perfRun = 0;
        g_perfHave = 0;
        return;
    }
    __try {
        if (where == measureAt) PerfMeasure();
        if (where == drawAt && g_perfOn) PerfDrawC(where);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_perfFail = 1;
        LogAnomaly("perf", "the performance overlay faulted (code %08lX, at the %s): off for this session", GetExceptionCode(),
                   where ? "post pass" : "frame end");
    }
}
