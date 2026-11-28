// v3.33 LOG333: the gameplay health log, framework level (the user: "Make a gameplay log that can tell you accurately if
// something seems off from now on. Use trace logs or verbose if you need to for debug levels"). The per-feature lines are
// the features' own; this file writes what no single feature can (staging\v333_feedback\w7_log\PLAN.md):
//   * the session header (once): this dll's version and md5, the exe's md5 (GOG v7 = 15a85581...), Playable Eileen's
//     dll version, dgVoodoo2 (d3d9.dll in the game folder), the display chain (first frame), every SH4Refined.ini value
//     that is not the default (and every hidden key the file holds), whether the ini can be written, the patch summary
//     (code ranges listed, byte checks that did not match, writes that failed, features whose install failed);
//   * a room line at every room change (verbose): time in the room (play / cutscene / paused), frame times (mean, p99,
//     worst with its context: the seconds into the room, Refined's own per-frame work and which feature's own timer
//     rose in that frame), spikes and stalls, the address space, and each feature that was on with how often it acted;
//     a load line when the game time jumps (a save, quick load or New Game: what that save brings: the Supply Drops
//     ledger, the pickup records free); the session summary at exit (always);
//   * framework ANOMALY checks: a long freeze in play, stutter in a room, a feature on for several rooms that never
//     acted where it must (Echo Doubles, After Shock, Supply Drops: the only ones decidable from their own state), a
//     feature turning fail-closed (fault), the address space nearing the 2 GB limit (the exe is not large-address
//     aware), the ini that cannot be written or a menu value that could not be saved, an install that failed or a
//     byte check that did not match at start, the Extra Menu's Quick Save row changing between grey and available
//     while one pause menu stays open, and a crash (the unhandled-exception filter, chained to the previous one).
// Game thread: LhFrameBegin / LhFrameEnd from the HUD call hook (items.inc SparkleFrame, every world frame, also paused).
// Reads the features' own globals (never writes them); every read of game memory is under SEH, fail-closed.
// v3.35 FX335 (staging/v335_feedback/w2_fx/PLAN.md; the user's log: "in play; Refined 34.71 ms" with no feature named):
//   the worst frame's context and the freeze lines name the frame's own top costs from budget.inc's ledger (every part of
//   Refined's per-frame work: the scene pass's parts, the post-filter draws, the depth fix, the tick's parts), "Refined" is
//   the ledger's total (the draw and the tick), the effect budget's level is noted, a frame within 3 s of a load (a game
//   time jump: the Extra Menu's Load Game, a quick load) says "after a load" and is not a freeze in play, and the effect
//   budget is fed here every frame (BgFrame) with its room line after the room's own (BgRoomLog).
// Log size (verbose, 60 fps, every feature on): 2 room lines of ~350 bytes a room change + at most 80 Quick Save row
// lines, 60 freeze lines and the anomalies a session: well under 0.1 MB an hour; trace (Level 3) adds a 1 s frame line,
// about 0.4 MB an hour. LogV / LogT stop after 32 MB of log in one session (LogLine counts the bytes; one note line).
#include "loghealth_sim.h"

#define LH_IDLE_ROOMS 3        // rooms in a row a decidable feature may stay idle before the ANOMALY
#define LH_IDLE_PLAY_S 20.0f   // a room counts for that only with this much play in it
#define LH_WALK_U 1500.0f      // After Shock: she walked this far in the room (units, horizontal), so a trail must show

typedef struct {
    LONG onF, actF;            // play frames with the row on / with it acting (per-frame features)
    LONG c0, cMax;             // a counter feature: its value at the room start, and the room's own total so far
} LhFeatRoom;
enum { LHF_ECHO, LHF_AI, LHF_DP, LHF_NC, LHF_COPIES, LHF_FOLLOW, LHF_DROPS, LHF_N };
static const char *const kLhFeat[LHF_N] = { "Echo", "After Shock", "Death Presence", "No Clip", "Enemy Copies",
                                            "Enemies Follow", "Supply Drops" };
static const char *const kLhActUnit[LHF_N] = { "f", "f", "f", "f", "(copies killed)", "(followers queued)", "(drops)" };

static int g_lhFail;                       // fail-closed: a fault in this file
static int g_lhHeaderDone, g_lhFirstFrameDone;
static LARGE_INTEGER g_lhQf, g_lhBeginQ, g_lhPrevQ, g_lhSessQ;
static int g_lhPrevPhase = -1;
static DWORD g_lhRoomKey = 0xFFFFFFFFu;
static LARGE_INTEGER g_lhRoomQ;            // the room's first frame
static double g_lhMsPlay, g_lhMsCut, g_lhMsPause;
static LhStats g_lhRoom, g_lhSess;
static float g_lhWorstAt, g_lhWorstOwn;    // the room's worst counted frame: seconds into the room, Refined's own ms
static char g_lhWorstWhy[128];
static float g_lhSessWorstAt;
static DWORD g_lhSessWorstKey = 0xFFFFFFFFu;
static char g_lhSessWorstWhy[128];
static LhRate g_lhFreezeRate;              // freeze ANOMALY lines: 2 a room
static volatile LONG g_lhFreezeQ;          // and 30 a session
static float g_lhPrevEchoMs, g_lhPrevAiMs, g_lhPrevDfxMs, g_lhPrevDpMs, g_lhPrevEchoTick;  // (v3.35 FX335: kept, no longer read)
static LARGE_INTEGER g_lhLoadAtQ;          // v3.35 FX335: the last load seen (LhLoadCheck: a game time jump)
static int g_lhLoadSeen, g_lhWorstLoad, g_lhPlayRun;
static LhFeatRoom g_lhF[LHF_N];
static LONG g_lhKill0, g_lhCopyKill0;      // the game's kill count and the copies killed at the room start
static int g_lhLedger0, g_lhDropCopy0;
static float g_lhWalk, g_lhPos[3];
static int g_lhPosOk;
static LhIdle g_lhIdle[LHF_N];
static int g_lhDropKills;                  // monster kills since the last drop (Supply Drops on, budget left)
static int g_lhDropFlagged;
// the session's feature totals (the summary): rooms on, rooms acted, acted total
static int g_lhFRoomsOn[LHF_N], g_lhFRoomsAct[LHF_N];
static LONG g_lhFTotal[LHF_N];
static int g_lhRooms, g_lhRoomKinds;
static DWORD g_lhSeen[64];                 // room keys seen (the summary counts distinct rooms)
// address space
static int g_lhVaLow, g_lhVaUsed, g_lhVaTotal, g_lhVaPeak;
static DWORD g_lhVaTick, g_lhFaultTick, g_lhTraceTick;
static LhStats g_lhTrace;                  // the trace's 1 s window
// faults
static int g_lhFaultSeen[8], g_lhEchoDrawFaults;
// the Quick Save row in one pause menu
static int g_lhInPause;
static LhFlip g_lhQsFlip;
static DWORD g_lhPauseTick;
static char g_lhQsWhy[256];
static volatile LONG g_lhQsQ;              // transition lines a session
static int g_lhQsFlipsRoom, g_lhQsAnom;
// ini
static int g_lhIniBad;
static int g_lhUnsavedSeen;
// a save load (the game time jumps): what the loaded save brings
static int g_lhGtPrev, g_lhGtHave;
static volatile LONG g_lhLoadQ;
// the crash filter
static LPTOP_LEVEL_EXCEPTION_FILTER g_lhPrevFilter;
static volatile LONG g_lhEnded;

static float g_lhOwnMs;             // v3.37 CP337: the last frame's own work (the value below), for perfhud.inc's readout
static volatile LONG g_lhOwnN;      // v3.37 CP337: +1 with each new g_lhOwnMs
static float LhMs(LONGLONG d) { return g_lhQf.QuadPart ? (float)((double)d * 1000.0 / (double)g_lhQf.QuadPart) : 0.0f; }

// ---------------------------------------------------------------- files: md5, module versions
static int LhFileMd5(const char *path, char hex[33], DWORD *size)
{
    static unsigned char buf[65536];
    LhMd5 m;
    unsigned char d[16];
    DWORD got, total = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    hex[0] = 0;
    if (size) *size = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    LhMd5Init(&m);
    while (ReadFile(h, buf, sizeof buf, &got, NULL) && got) {
        LhMd5Update(&m, buf, got);
        total += got;
    }
    CloseHandle(h);
    LhMd5Final(&m, d);
    LhHex(d, 16, hex);
    if (size) *size = total;
    return 1;
}

// FILEVERSION a.b.c.d of a loaded module's version resource ("" = none)
static void LhModVer(HMODULE mod, char *out, size_t n)
{
    HRSRC r;
    HGLOBAL g;
    const BYTE *p;
    DWORD sz, i;
    out[0] = 0;
    if (!mod) return;
    __try {
        r = FindResourceA(mod, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);
        g = r ? LoadResource(mod, r) : NULL;
        p = g ? (const BYTE *)LockResource(g) : NULL;
        sz = r ? SizeofResource(mod, r) : 0;
        for (i = 0; p && i + 52 <= sz; i += 4)
            if (*(const DWORD *)(p + i) == 0xFEEF04BD) {
                const DWORD ms = *(const DWORD *)(p + i + 8), ls = *(const DWORD *)(p + i + 12);
                sprintf_s(out, n, "%lu.%lu.%lu.%lu", ms >> 16, ms & 0xFFFF, ls >> 16, ls & 0xFFFF);
                return;
            }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[0] = 0;
    }
}

// "game folder" / "system" / the folder, for a loaded module
static void LhModWhere(HMODULE mod, char *out, size_t n)
{
    char p[MAX_PATH], sys[MAX_PATH], *s;
    UINT k;
    out[0] = 0;
    if (!GetModuleFileNameA(mod, p, MAX_PATH)) return;
    s = strrchr(p, '\\');
    if (s) s[1] = 0;
    if (!_stricmp(p, g_dir)) {
        strcpy_s(out, n, "game folder");
        return;
    }
    k = GetSystemWow64DirectoryA(sys, MAX_PATH);
    if (!k || k >= MAX_PATH) k = GetSystemDirectoryA(sys, MAX_PATH);
    if (k && k < MAX_PATH - 1) {
        strcat_s(sys, sizeof sys, "\\");
        if (!_stricmp(p, sys)) {
            strcpy_s(out, n, "system");
            return;
        }
    }
    strcpy_s(out, n, p);
}

// ---------------------------------------------------------------- the session header (end of Init)
static void LhIniDefaults(void)
{
    static char sec[16384], names[2048], line[700];
    char v[256];
    const char *s;
    int k, o = 0, parts = 0, n = 0;
    line[0] = 0;
#define LH_ADD(fmt, a, b, c)                                                                    \
    do {                                                                                        \
        char one[320];                                                                          \
        int m = sprintf_s(one, sizeof one, fmt, a, b, c);                                       \
        if (m > 0 && o + m + 2 >= (int)sizeof line - 1) {                                       \
            Log("settings not default (%d): %s", ++parts, line);                                \
            o = 0, line[0] = 0;                                                                 \
        }                                                                                       \
        if (m > 0) o += sprintf_s(line + o, sizeof line - o, "%s%s", o ? "; " : "", one), ++n;  \
    } while (0)
    for (k = 0; k < NKEYS; ++k) {
        const char *key = kIniKeys[k].key;
        if (!strcmp(kIniKeys[k].section, "Window") && (!strcmp(key, "Monitor") || !strcmp(key, "MonitorName") ||
                                                        !strcmp(key, "MonitorPath")))
            continue;  // written by the mod itself (the "settings: Monitor" line names them)
        if (g_iniFallback) strcpy_s(v, sizeof v, g_vals[k]);
        else GetPrivateProfileStringA(kIniKeys[k].section, key, "\x01", v, sizeof v, g_iniPath);
        if (!strcmp(v, "\x01") || LhIniSame(v, kIniKeys[k].def)) continue;
        LH_ADD("[%s] %s=%s", kIniKeys[k].section, key, v);
    }
    // keys of the file this table does not know: hidden keys (and keys of other versions)
    if (!g_iniFallback && GetPrivateProfileSectionNamesA(names, sizeof names, g_iniPath)) {
        for (s = names; *s; s += strlen(s) + 1) {
            const char *e;
            if (!GetPrivateProfileSectionA(s, sec, sizeof sec, g_iniPath)) continue;
            for (e = sec; *e; e += strlen(e) + 1) {
                char key[96];
                const char *eq = strchr(e, '=');
                size_t kl = eq ? (size_t)(eq - e) : 0;
                int known = 0;
                if (!eq || !kl || kl >= sizeof key || *e == ';') continue;
                memcpy(key, e, kl);
                key[kl] = 0;
                for (k = 0; k < NKEYS && !known; ++k)
                    known = !_stricmp(kIniKeys[k].section, s) && !_stricmp(kIniKeys[k].key, key);
                if (!known) LH_ADD("[%s] %s=%s (hidden key)", s, key, eq + 1);
            }
        }
    }
#undef LH_ADD
    if (o) Log("settings not default%s%s: %s", parts ? " " : "", parts ? "(last)" : "", line);
    else if (!n) Log("settings not default: none (every key of SH4Refined.ini has its default)");
}

static void LhIniWritable(void)
{
    const DWORD a = GetFileAttributesA(g_iniPath);
    HANDLE h;
    if (g_iniFallback) {
        LogAnomaly("ini", "SH4Refined.ini could not be written at the start: this session uses its values, but nothing "
                          "changed in the menus is kept");
        g_lhIniBad = 1;
        return;
    }
    if (a == INVALID_FILE_ATTRIBUTES) return;  // (a later IniInt writes it)
    if (a & FILE_ATTRIBUTE_READONLY) {
        LogAnomaly("ini", "SH4Refined.ini is read-only: menu changes will not be kept");
        g_lhIniBad = 1;
        return;
    }
    h = CreateFileA(g_iniPath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                    0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        LogAnomaly("ini", "SH4Refined.ini cannot be opened for writing (error %lu; Controlled Folder Access or file "
                          "rights?): menu changes will not be kept", GetLastError());
        g_lhIniBad = 1;
        return;
    }
    CloseHandle(h);  // nothing written
}

static void LhInstallChecks(void)
{
    static const struct { const char *name; const int *flag; const int *cond; } kInst[] = {
        { "Echo Doubles (echo.inc)", &g_echoInstalled, NULL },
        { "After Shock (afterimage.inc)", &g_aiInstalled, NULL },
        { "Death Presence (deathpresence.inc)", &g_dpInstalled, NULL },
        { "Enemy Copies / Follow / Revive (enemies.inc)", &g_enmInstalled, NULL },
        { "Supply Drops (drops.inc)", &g_dropInstalled, NULL },
        { "No Clip (noclip.inc)", &g_ncReady, NULL },
        { "the HUD call hook (Item Shine, every per-frame draw, this log's room lines)", &g_spInstalled, NULL },
        { "Screen Effects (screenfx.inc)", &g_fxOn, NULL },
        { "Save Banks (savebanks.inc)", &g_sbInstalled, NULL },
        { "Quick Save (quicksave.inc)", &g_qsInstalled, &g_qsEnabled },
        { "the downed-ghost pickup (pickup.inc)", &g_pkInstalled, &g_pkOn },
        { "the larger task pool ([Gameplay] TaskPool, pools.inc)", &g_tpGrown, &g_tpWanted },  // v3.36 TP336
        { "the larger IK pool ([Gameplay] IkPool, pools.inc)", &g_ikGrown, &g_ikWanted },     // v3.36 TP336
    };
    char bad[512];
    int i, o = 0, nBad = 0, nOk = 0;
    bad[0] = 0;
    for (i = 0; i < (int)(sizeof kInst / sizeof kInst[0]); ++i) {
        if (kInst[i].cond && !*kInst[i].cond) continue;
        if (*kInst[i].flag) {
            ++nOk;
            continue;
        }
        ++nBad;
        LogAnomaly("install", "%s is not installed: its rows do nothing this session (its own line above says which "
                              "site differs or failed)", kInst[i].name);
        if (o < (int)sizeof bad - 80) o += sprintf_s(bad + o, sizeof bad - o, "%s%s", o ? ", " : "", kInst[i].name);
    }
    if (g_emEnabled && !g_extraTick) {
        ++nBad;
        LogAnomaly("install", "the Extra Menu ([ExtraMenu] Enabled=1) is not installed: the game's own pause menu");
    } else if (g_emEnabled) {
        ++nOk;
    }
    LogV("patches: %d of %d core features installed%s%s", nOk, nOk + nBad, nBad ? "; not: " : "", bad);
}

static void LhSessionHeader(void)
{
    char path[MAX_PATH], hex[33], ver[48];
    DWORD size = 0;
    HMODULE ds;
    if (g_lhHeaderDone) return;
    g_lhHeaderDone = 1;
    QueryPerformanceFrequency(&g_lhQf);
    QueryPerformanceCounter(&g_lhSessQ);
    GetModuleFileNameA(g_self, path, MAX_PATH);
    LhFileMd5(path, hex, &size);
    LhModVer(g_self, ver, sizeof ver);
    Log("session: SH4 Refined v%s (file version %s), winmm.dll md5 %s, %lu bytes, built %s %s; log level %d", SH4WM_VERSION,
        ver[0] ? ver : "?", hex[0] ? hex : "(unreadable)", size, __DATE__, __TIME__, g_logLevel);
    GetModuleFileNameA(NULL, path, MAX_PATH);
    if (LhFileMd5(path, hex, &size)) {
        if (!strcmp(hex, "15a85581eca3d0664089c440b0acada1"))
            Log("session: exe SILENT HILL 4.exe md5 %s (GOG v7, the build every site is made for), %lu bytes, base %p", hex,
                size, g_exe);
        else
            LogAnomaly("exe", "SILENT HILL 4.exe md5 %s (%lu bytes) is not GOG v7 (15a85581eca3d0664089c440b0acada1): byte "
                              "checks may refuse sites or patch the wrong code", hex, size);
    } else {
        Log("session: exe %s could not be read for its md5 (error %lu)", path, GetLastError());
    }
    ds = GetModuleHandleA("dsound.dll");
    if (ds && HasExport(ds, "SH4PE_Patches")) {
        LhModVer(ds, ver, sizeof ver);
        Log("session: Playable Eileen loaded (dsound.dll v%s)", ver[0] ? ver : "?");
    } else {
        Log("session: Playable Eileen not loaded (no dsound.dll with SH4PE_Patches yet)");
    }
    sprintf_s(path, sizeof path, "%sd3d9.dll", g_dir);
    {
        const DWORD a = GetFileAttributesA(path);
        char conf[MAX_PATH];
        sprintf_s(conf, sizeof conf, "%sdgVoodoo.conf", g_dir);
        if (a != INVALID_FILE_ATTRIBUTES)
            Log("session: d3d9.dll in the game folder (dgVoodoo2 when dgVoodoo.conf is there: %s): Direct3D goes GOG's "
                "d3d8.dll -> ori_d3d8.dll -> that d3d9.dll; versions in the display line at the first frame",
                GetFileAttributesA(conf) != INVALID_FILE_ATTRIBUTES ? "present" : "missing");
        else
            Log("session: no d3d9.dll in the game folder (no dgVoodoo2): GOG's d3d8.dll -> ori_d3d8.dll -> the system d3d9");
    }
    LhIniDefaults();
    LhIniWritable();
    {
        char list[200];
        int i, o = 0;
        list[0] = 0;
        for (i = 0; i < g_lhSiteBadN && i < LH_SITE_KEEP; ++i)
            o += sprintf_s(list + o, sizeof list - o, "%s%08lX", i ? " " : "", g_lhSiteBad[i]);
        Log("patches: %lu code range(s) listed, %ld byte check(s) at the start, %d did not match%s%s%s, %ld write(s) "
            "failed", SH4WM_Patches.count, g_lhSiteChecks, g_lhSiteBadN, g_lhSiteBadN ? " (" : "", list,
            g_lhSiteBadN ? (g_lhSiteBadN > LH_SITE_KEEP ? " ...)" : ")") : "", g_lhPokeFails);
        if (g_lhSiteBadN && g_exe == (HMODULE)0x400000)
            LogAnomaly("install", "%d byte check(s) did not match the GOG v7 exe at the start (%s): those sites were left "
                                  "stock (another mod, or an exe that is not stock); the lines above say which feature is off",
                       g_lhSiteBadN, list);
        if (g_lhPokeFails)
            LogAnomaly("install", "%ld code write(s) failed (VirtualProtect): those sets were rolled back", g_lhPokeFails);
    }
    g_lhSiteOpen = 0;  // later byte checks (runtime probes) are not counted
    if (g_exe == (HMODULE)0x400000) LhInstallChecks();
}

// ---------------------------------------------------------------- v3.59 SD359: the fault watch (faultwhere_sim.h)
// LOGCHECK_v358.md section 2: at 22:34 Refined's warm-up and shadow composite guards swallowed two faults and logged only a
// code; the crash that followed was in dgVoodoo's d3d9.dll after an AMD driver event. Every one of Refined's ~260 __try
// guards is covered without touching them: a first-chance vectored handler (hidden [Log] FaultWhere=1) notes each error
// exception (0xC...: never breakpoints, guard pages or C++ throws) in a ring of 16: code, address, the image it lies in (by
// VirtualQuery: no loader lock inside the handler), the thread, an access violation's target. It never handles anything
// (EXCEPTION_CONTINUE_SEARCH). The game thread's frame end logs the ring: "fault watch: ... at d3d9.dll+0007843A ...: inside
// Direct3D or the graphics driver" (24 lines a session, then counted; the session's end line sums them by module). The
// warm-up and the shadow composite also name themselves as the guard that caught it and put the place in their own line.
// The Direct3D stop (hidden [Graphics] D3DFaultStop=1): two faults within a second on the game thread inside Direct3D or the
// driver (or inside one of Refined's Direct3D guards) turn Refined's own Direct3D work off for the session through each
// feature's own fail-closed path: the warm-up, Shadow Edge, Depth of Field, the post effects, Echo Doubles, After Shock,
// Death Presence, Fade In and the model glow. The game draws on as it would without them.
#include "faultwhere_sim.h"
#define LH_RING 16
#define LH_GFX_MAX 24
typedef struct { volatile LONG seq; DWORD code, tick, tid, rw; const void *at, *base; ULONG_PTR addr; DWORD nparm; char guard[24]; } LhFault;
static LhFault g_lhRing[LH_RING];
static volatile LONG g_lhRingW;          // faults noted (the ring's write count)
static LONG g_lhRingR;                   // logged (the game thread)
static PVOID g_lhVeh;
static int g_lhFwOn = 1, g_lhD3dStopOn = 1;
static volatile DWORD g_lhGameTid;       // the thread of LhFrameEndC
static ULONG_PTR g_lhGfxLo[LH_GFX_MAX], g_lhGfxHi[LH_GFX_MAX];
static volatile LONG g_lhGfxN;
static DWORD g_lhGfxTick;
static FwD3d g_lhD3d;                    // the game thread only
static volatile LONG g_lhD3dTrip;        // two Direct3D faults: acted on at the frame end
static DWORD g_lhD3dCode;
static const void *g_lhD3dAt;
static int g_lhD3dStopped;
static LONG g_lhFwKinds[FW_N], g_lhFwLogged, g_lhFwLost;
static char g_lhFwMod[6][32];
static LONG g_lhFwModN[6];
static DWORD g_lhSeenCode;               // the last fault a named guard caught (the game thread)
static const void *g_lhSeenAt;
static char g_lhSeenText[96];
static int LhIsGfxAddr(ULONG_PTR a)
{
    LONG i;
    const LONG n = g_lhGfxN;
    for (i = 0; i < n && i < LH_GFX_MAX; ++i)
        if (a >= g_lhGfxLo[i] && a < g_lhGfxHi[i]) return 1;
    return 0;
}
static const char *LhBaseName(const char *path) { const char *s = strrchr(path, '\\'); return s ? s + 1 : path; }
static void LhGfxScan(void)  // the game thread: the display chain's and the drivers' images (the first frame, then each minute)
{
    typedef BOOL(WINAPI * Enum_t)(HANDLE, HMODULE *, DWORD, DWORD *);
    static Enum_t s_en;
    static HMODULE mods[512];
    DWORD need = 0, i, n;
    if (!s_en) s_en = (Enum_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32EnumProcessModules");
    if (!s_en || !s_en(GetCurrentProcess(), mods, sizeof mods, &need)) return;
    n = need / sizeof(HMODULE);
    if (n > 512) n = 512;
    for (i = 0; i < n && g_lhGfxN < LH_GFX_MAX; ++i) {
        char path[MAX_PATH];
        DWORD lo = 0, hi = 0;
        LONG k;
        int have = 0;
        if (!GetModuleFileNameA(mods[i], path, MAX_PATH) || !FwGfxModule(LhBaseName(path))) continue;
        CamModRange(mods[i], &lo, &hi);
        for (k = 0; k < g_lhGfxN; ++k) have |= g_lhGfxLo[k] == lo;
        if (have || hi <= lo) continue;
        k = g_lhGfxN;
        g_lhGfxLo[k] = lo, g_lhGfxHi[k] = hi;
        MemoryBarrier();
        InterlockedIncrement(&g_lhGfxN);  // the handler reads entries below the count only
    }
}
static void LhD3dCount(DWORD code, const void *at)  // the game thread
{
    if (FwD3dStep(&g_lhD3d, GetTickCount())) {
        g_lhD3dCode = code, g_lhD3dAt = at;
        InterlockedExchange(&g_lhD3dTrip, 1);
    }
}
static LONG CALLBACK LhFaultVeh(EXCEPTION_POINTERS *e)  // any thread, first chance: note, never handle
{
    const EXCEPTION_RECORD *const r = e ? e->ExceptionRecord : NULL;
    MEMORY_BASIC_INFORMATION mb;
    LhFault *f;
    LONG w;
    DWORD tid;
    if (!r || !FwNoted(r->ExceptionCode)) return EXCEPTION_CONTINUE_SEARCH;
    w = InterlockedIncrement(&g_lhRingW) - 1;
    f = &g_lhRing[w % LH_RING];
    InterlockedExchange(&f->seq, 0);
    f->code = r->ExceptionCode, f->at = r->ExceptionAddress, f->tick = GetTickCount();
    f->tid = tid = GetCurrentThreadId();
    f->base = VirtualQuery(f->at, &mb, sizeof mb) == sizeof mb && mb.Type == MEM_IMAGE ? mb.AllocationBase : NULL;
    f->nparm = r->NumberParameters;
    f->rw = f->nparm >= 2 ? (DWORD)r->ExceptionInformation[0] : 0;
    f->addr = f->nparm >= 2 ? r->ExceptionInformation[1] : 0;
    f->guard[0] = 0;
    InterlockedExchange(&f->seq, w + 1);
    if (g_lhGameTid && tid == g_lhGameTid && LhIsGfxAddr((ULONG_PTR)f->at)) LhD3dCount(f->code, f->at);
    return EXCEPTION_CONTINUE_SEARCH;
}
// "C0000005 at d3d9.dll+0007843A" (outside the handler: the loader is asked for the name)
static void LhWhereText(DWORD code, const void *at, char *out, size_t n)
{
    MEMORY_BASIC_INFORMATION mb;
    char path[MAX_PATH];
    const void *base = VirtualQuery(at, &mb, sizeof mb) == sizeof mb && mb.Type == MEM_IMAGE ? mb.AllocationBase : NULL;
    if (base && GetModuleFileNameA((HMODULE)base, path, MAX_PATH))
        sprintf_s(out, n, "%08lX at %s+%08lX", code, LhBaseName(path), (DWORD)((const BYTE *)at - (const BYTE *)base));
    else
        sprintf_s(out, n, "%08lX at %p", code, at);
}
// the __except filter of a named guard (the warm-up, the shadow composite): the fault's place noted, the guard named in the
// watch's entry; a fault of Refined's own Direct3D work outside the driver counts for the Direct3D stop too
static int LhFaultSeen(EXCEPTION_POINTERS *e, const char *guard)
{
    const EXCEPTION_RECORD *const r = e && e->ExceptionRecord ? e->ExceptionRecord : NULL;
    const LONG w = g_lhRingW;
    const DWORD tid = GetCurrentThreadId();
    LONG k;
    if (!r) return EXCEPTION_EXECUTE_HANDLER;
    g_lhSeenCode = r->ExceptionCode, g_lhSeenAt = r->ExceptionAddress, g_lhSeenText[0] = 0;
    for (k = w - 1; k >= 0 && k >= w - 4; --k) {
        LhFault *const f = &g_lhRing[k % LH_RING];
        if (f->seq == k + 1 && f->at == r->ExceptionAddress && f->tid == tid) {
            strncpy_s(f->guard, sizeof f->guard, guard, _TRUNCATE);
            break;
        }
    }
    if (g_lhGameTid && tid == g_lhGameTid && !LhIsGfxAddr((ULONG_PTR)r->ExceptionAddress)) LhD3dCount(r->ExceptionCode, r->ExceptionAddress);
    return EXCEPTION_EXECUTE_HANDLER;
}
static const char *LhFaultText(void)  // in the guard's handler body: the fault its filter saw
{
    if (!g_lhSeenText[0]) LhWhereText(g_lhSeenCode, g_lhSeenAt, g_lhSeenText, sizeof g_lhSeenText);
    return g_lhSeenText;
}
static void LhFwModAdd(const char *mod)
{
    int i;
    for (i = 0; i < 6 && g_lhFwMod[i][0]; ++i)
        if (!_stricmp(g_lhFwMod[i], mod)) {
            ++g_lhFwModN[i];
            return;
        }
    if (i < 6) strncpy_s(g_lhFwMod[i], sizeof g_lhFwMod[i], mod, _TRUNCATE), g_lhFwModN[i] = 1;
}
static void LhFaultLog(const LhFault *f)
{
    char mod[MAX_PATH] = "?", acc[48] = "", who[40], guard[64] = "";
    const char *name = "?";
    int kind;
    if (f->base && GetModuleFileNameA((HMODULE)f->base, mod, MAX_PATH)) name = LhBaseName(mod);
    kind = FwKind(f->base != NULL, f->base == (const void *)g_self, f->base == (const void *)0x400000, f->base && FwGfxModule(name));
    ++g_lhFwKinds[kind];
    LhFwModAdd(f->base ? name : "(none)");
    if (++g_lhFwLogged > 24) {
        if (g_lhFwLogged == 25) Log("fault watch: 24 faults logged this session: the rest are counted (the session's end line)");
        return;
    }
    if (f->code == 0xC0000005ul && f->nparm >= 2)
        sprintf_s(acc, sizeof acc, " (%s of %08lX)", f->rw == 0 ? "read" : f->rw == 1 ? "write" : f->rw == 8 ? "execute" : "access",
                  (unsigned long)f->addr);
    if (f->tid == g_lhGameTid) strcpy_s(who, sizeof who, "the game thread");
    else sprintf_s(who, sizeof who, "thread %lu", f->tid);
    if (f->guard[0]) sprintf_s(guard, sizeof guard, "; Refined's %s guard caught it", f->guard);
    Log("fault watch: %08lX %s%s at %s+%08lX (%p), %s, %lu ms ago: %s%s", f->code, FwCodeName(f->code), acc, f->base ? name : "?",
        f->base ? (DWORD)((const BYTE *)f->at - (const BYTE *)f->base) : (DWORD)(ULONG_PTR)f->at, f->at, who,
        GetTickCount() - f->tick, kFwKind[kind], guard);
}
static void LhFaultDrain(void)  // the game thread's frame end (and the session's end)
{
    const LONG w = g_lhRingW;
    if (g_lhRingR == w) return;
    if (w - g_lhRingR > LH_RING) {
        g_lhFwLost += w - g_lhRingR - LH_RING;
        g_lhRingR = w - LH_RING;
    }
    while (g_lhRingR < w) {
        const LhFault *const f = &g_lhRing[g_lhRingR % LH_RING];
        LhFault c;
        if (f->seq != g_lhRingR + 1) break;  // still being written: the next frame
        c = *f;
        if (InterlockedCompareExchange((volatile LONG *)&f->seq, 0, 0) != g_lhRingR + 1) {
            ++g_lhFwLost;  // written over meanwhile
            ++g_lhRingR;
            continue;
        }
        ++g_lhRingR;
        LhFaultLog(&c);
    }
}
static void LhD3dStop(void)  // the frame end after the trip: each feature's own fail-closed path, each in its own guard
{
    static const char kWhy[] = "Direct3D faults (two in a second)";
    char where[128];
    LhWhereText(g_lhD3dCode, g_lhD3dAt, where, sizeof where);
    if (!g_lhD3dStopOn) {
        Log("direct3d: two faults within a second (the last %s): [Graphics] D3DFaultStop=0: Refined's own Direct3D work goes on", where);
        return;
    }
    LogAnomaly("direct3d", "two faults within a second inside Direct3D / the graphics driver or Refined's Direct3D work (the last %s): "
                           "Refined's own Direct3D work is off for this session (the warm-up, Shadow Edge, Depth of Field, the post effects, "
                           "Echo Doubles, After Shock, Death Presence, Fade In, the model glow); the game draws on", where);
    g_rfWarmFaults = 3;
    g_gvFaults = 3;
    __try { ShFail(SHF_FAULT, E_FAIL, kWhy); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { DofFail(kWhy, E_FAIL); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { PpFail(PPE_BLOOM, kWhy, E_FAIL), PpFail(PPE_GRADE, kWhy, E_FAIL), PpFail(PPE_LENS, kWhy, E_FAIL); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try {
        g_echoFaultCode = g_lhD3dCode, g_echoFaultAt = (void *)g_lhD3dAt;
        EchoFailNow(kWhy);
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { AiFailNow(kWhy, g_lhD3dCode, g_lhD3dAt); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { DpFailNow(kWhy, g_lhD3dCode, g_lhD3dAt); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try {
        g_fiFaultCode = g_lhD3dCode, g_fiFaultAt = (void *)g_lhD3dAt;
        FiFailNow(kWhy);
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
static void LhFaultWatchStart(void)  // the first frame (the game thread)
{
    g_lhFwOn = GetPrivateProfileIntA("Log", "FaultWhere", 1, g_iniPath) != 0;         // hidden
    g_lhD3dStopOn = GetPrivateProfileIntA("Graphics", "D3DFaultStop", 1, g_iniPath) != 0;  // hidden
    if (!g_lhFwOn) {
        Log("session: fault watch off ([Log] FaultWhere=0): a guard's fault is logged by its code only; no Direct3D stop");
        return;
    }
    g_lhGameTid = GetCurrentThreadId();  // (none while off: LhFaultSeen counts nothing then)
    LhGfxScan();
    g_lhGfxTick = GetTickCount();
    g_lhVeh = AddVectoredExceptionHandler(1, LhFaultVeh);  // this dll is pinned (Init): never unloaded under it
    Log("session: fault watch %s: each fault's code, address and module (%ld Direct3D / driver image(s) known); two Direct3D faults "
        "within a second %s", g_lhVeh ? "in" : "NOT IN (no vectored handler)", g_lhGfxN,
        g_lhD3dStopOn ? "stop Refined's own Direct3D work for the session" : "are only logged ([Graphics] D3DFaultStop=0)");
}
static void LhFaultEndLine(void)  // the session's end
{
    char mods[200];
    int i, o = 0;
    const LONG all = g_lhFwKinds[0] + g_lhFwKinds[1] + g_lhFwKinds[2] + g_lhFwKinds[3] + g_lhFwKinds[4];
    if (!all && !g_lhFwLost) return;
    mods[0] = 0;
    for (i = 0; i < 6 && g_lhFwMod[i][0] && o < (int)sizeof mods - 40; ++i)
        o += sprintf_s(mods + o, sizeof mods - (size_t)o, "%s%s %ld", i ? ", " : "", g_lhFwMod[i], g_lhFwModN[i]);
    Log("session: faults seen %ld (inside SH4 Refined %ld, the game %ld, Direct3D / the driver %ld, other %ld, none %ld; %ld not read): %s; "
        "Refined's Direct3D work %s", all + g_lhFwLost, g_lhFwKinds[FW_REFINED], g_lhFwKinds[FW_GAME], g_lhFwKinds[FW_GFX],
        g_lhFwKinds[FW_OTHER], g_lhFwKinds[FW_UNKNOWN], g_lhFwLost, mods[0] ? mods : "-", g_lhD3dStopped ? "stopped" : "kept");
}

// ---------------------------------------------------------------- the crash filter (chained)
static void LhSessionEnd(const char *why);
static LONG WINAPI LhCrashFilter(EXCEPTION_POINTERS *e)
{
    if (e && e->ExceptionRecord && InterlockedCompareExchange(&g_lhEnded, 0, 0) == 0) {
        const void *at = e->ExceptionRecord->ExceptionAddress;
        HMODULE m = NULL;
        char mod[MAX_PATH] = "?", *s;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)at, &m) && m) {
            GetModuleFileNameA(m, mod, MAX_PATH);
            s = strrchr(mod, '\\');
            if (s) memmove(mod, s + 1, strlen(s));
        }
        if (g_logCsInit == 2 && TryEnterCriticalSection(&g_logCs)) {
            LogAnomaly("crash", "exception %08lX at %p (%s+%lX), thread %lu, stage %d room %d: the game is going down",
                       e->ExceptionRecord->ExceptionCode, at, mod, (DWORD)((const BYTE *)at - (const BYTE *)m),
                       GetCurrentThreadId(), (int)*(volatile DWORD *)0xFD5A90, (int)*(volatile DWORD *)0xFD5A94);
            LeaveCriticalSection(&g_logCs);
            LhSessionEnd("crash");
        }
    }
    return g_lhPrevFilter ? g_lhPrevFilter(e) : EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------- the first world frame: the display chain
static void LhFirstFrame(void)
{
    static const char *const kMods[] = { "d3d8.dll", "ori_d3d8.dll", "d3d9.dll", "d3d11.dll", "d3d12.dll", "dxgi.dll" };
    char line[700], ver[48], where[MAX_PATH];
    int i, o, gw = *(volatile int *)0x5D524C, gh = *(volatile int *)0x5D5250, rate = *(volatile int *)0x5D5248;
    RECT r = { 0, 0, 0, 0 };
    HMODULE ds;
    const char *note = "";  // v3.41 VW341
    if (g_hwnd) GetClientRect(g_hwnd, &r);
    if (g_hwnd && g_dm == DM_BORDERLESS) {  // v3.41 VW341: the v3.40 log's "window client 800x627": read before the placement
        MONITORINFO mi;
        mi.cbSize = sizeof mi;
        if (GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi) &&
            (r.right != mi.rcMonitor.right - mi.rcMonitor.left || r.bottom != mi.rcMonitor.bottom - mi.rcMonitor.top))
            note = " (not placed yet: the 'window: borderless' line follows)";
    }
    o = sprintf_s(line, sizeof line, "%s, game size %dx%d, window client %ldx%ld%s, gameplay rate %d (FrameMode %d); chain:",
                  kDmName[g_dm >= 0 && g_dm < DM_N ? g_dm : 0], gw, gh, r.right, r.bottom, note, rate, g_frameMode);
    for (i = 0; i < (int)(sizeof kMods / sizeof kMods[0]); ++i) {
        HMODULE m = GetModuleHandleA(kMods[i]);
        if (!m) continue;
        LhModVer(m, ver, sizeof ver);
        LhModWhere(m, where, sizeof where);
        if (o > 0 && o < (int)sizeof line - 120)
            o += sprintf_s(line + o, sizeof line - o, " %s (%s%s%s)", kMods[i], where, ver[0] ? ", v" : "", ver);
    }
    Log("display: %s", line);
    ds = GetModuleHandleA("dsound.dll");
    if (ds && HasExport(ds, "SH4PE_Patches")) {
        LhModVer(ds, ver, sizeof ver);
        LogV("session: Playable Eileen dsound.dll v%s at the first frame", ver[0] ? ver : "?");
    }
    // after the game's and Playable Eileen's own filters (PE writes its crash dump: chained)
    g_lhPrevFilter = SetUnhandledExceptionFilter(LhCrashFilter);
    LogV("session: crash line filter in (chained to %p)", g_lhPrevFilter);
    LhFaultWatchStart();  // v3.59 SD359: the fault watch (each fault's module) and the Direct3D stop
}

// ---------------------------------------------------------------- features: on / acting (reads their own state)
static int LhFeatOn(int f)
{
    switch (f) {
    case LHF_ECHO: return g_echoReady && !g_echoFail && g_echoMode > 0;
    case LHF_AI: return g_aiReady && !g_aiFail && g_aiLevel > 0;
    case LHF_DP: return g_dpReady && !g_dpFail && g_dpLevel > 0;
    case LHF_NC: return g_ncReady && !g_ncFault && g_ncSet > 0;
    case LHF_COPIES: return g_enmCopiesReady && !g_enmFail && g_enmCopies > 0;
    case LHF_FOLLOW: return g_enmFollowReady && !g_enmFail && g_enmFollow > 0;
    case LHF_DROPS: return g_dropReady && !g_dropFail && g_dropLevel > 0;
    }
    return 0;
}
static const char *LhGpName(int k, LONG v)
{
    return v >= 0 && v < GP_VMAX && kGpName[k][v] ? kGpName[k][v] : "?";
}
static void LhFeatValue(int f, char *out, size_t n)
{
    switch (f) {
    case LHF_ECHO: sprintf_s(out, n, "%s x%ld", LhGpName(GP_ECHO, g_echoMode), g_echoCountV + 1); return;
    case LHF_AI: strcpy_s(out, n, LhGpName(GP_AI, g_aiLevel)); return;
    case LHF_DP: strcpy_s(out, n, LhGpName(GP_DP, g_dpLevel)); return;
    case LHF_NC:
        strcpy_s(out, n, g_ncSet >= 0 && g_ncSet < (LONG)(sizeof kNcName / sizeof kNcName[0]) ? kNcName[g_ncSet] : "?");
        return;
    case LHF_COPIES: strcpy_s(out, n, LhGpName(GP_COPIES, g_enmCopies)); return;
    case LHF_FOLLOW: strcpy_s(out, n, LhGpName(GP_FOLLOW, g_enmFollow)); return;
    case LHF_DROPS: strcpy_s(out, n, LhGpName(GP_DROP, g_dropLevel)); return;
    }
    out[0] = 0;
}
static int LhLedgerSum(void)  // drops given in all worlds and visits, from the saved ledger (bytes 1..7, two nibbles)
{
    const BYTE *L = (const BYTE *)DROP_LEDGER_VA;
    int i, s = 0;
    for (i = 1; i < 8; ++i) s += (L[i] & 15) + (L[i] >> 4);
    return s;
}
static int LhLedgerHere(int *budget)  // drops this world visit, and its budget
{
    const int stage = (int)*(volatile DWORD *)0xFD5A90, visit = (*(volatile const BYTE *)0x13D812D >> 2) & 1;
    *budget = g_dropLevel > 0 && g_dropLevel < DROP_LEVELS && !g_dropAgeOn ? kDropRates[g_dropLevel].worldBudget : 0;  // v3.35 MERGE: 0 = no budget (drops fade)
    return DropLedgerCount((const DropLedger *)DROP_LEDGER_VA, stage, visit);
}

static int LhPickupFree(int *ours)  // the game's 32 dynamic pickup records (saved whole): free ones, and Supply Drops' own
{
    DropRecView v[DROP_REC_MAX];  // v3.35 PK335
    int i, maxSeq, freeN = 0;
    DropRecViews(v, &maxSeq);
    *ours = 0;
    for (i = 0; i < DROP_REC_N; ++i) {
        if (!v[i].state) ++freeN;
        else if (v[i].ours) ++*ours;
    }
    return freeN;
}

// A save load, a quick load, a New Game or the first world frame: the game time went back or jumped ahead (10 s and
// more at once). One line with what the loaded save brings (the user: "certain saves may impede some mods").
static void LhLoadCheck(void)
{
    const int t = *(volatile int *)0xFD5BD0;  // gGame.time, game frames
    const int prev = g_lhGtPrev, had = g_lhGtHave;
    int ours, freeN, budget, here;
    const BYTE *L = (const BYTE *)DROP_LEDGER_VA;
    g_lhGtPrev = t;
    g_lhGtHave = 1;
    if (had && t >= prev && t - prev <= 600) return;
    QueryPerformanceCounter(&g_lhLoadAtQ);  // v3.35 FX335: the next 3 s are the load's, not play
    g_lhLoadSeen = 1;
    freeN = LhPickupFree(&ours);
    here = LhLedgerHere(&budget);
    if (LogQuota(&g_lhLoadQ, 200))
        LogV("load: game time %d (%s %d), stage %X room %X, difficulty %d, kills %ld; the save's Supply Drops ledger "
             "%02X %02X%02X%02X%02X%02X%02X%02X (this world visit %d%s%.0d), pickup records free %d of %d (%d of them drops)",  // v3.35 MERGE: no budget while drops fade
             t, had ? "was" : "first world frame; was", prev, (int)*(volatile DWORD *)0xFD5A90, (int)*(volatile DWORD *)0xFD5A94,
             *(volatile int *)0xFD5A84, *(volatile LONG *)0xFD5BD8, L[0], L[1], L[2], L[3], L[4], L[5], L[6], L[7], here,
             budget ? " of " : ", no budget: drops fade", budget, freeN, DROP_REC_N, ours);
    if (LhFeatOn(LHF_DROPS) && g_dropLevel > 0 && g_dropLevel < DROP_LEVELS && freeN < kDropRates[g_dropLevel].minFree + 1)
        LogAnomaly("supply drops", "the loaded game holds %d of %d pickup records (%d free, %d of them drops): Supply "
                                   "Drops needs %d free to place a drop, so none appears until records free up (stage %X "
                                   "room %X, game time %d)", DROP_REC_N - freeN, DROP_REC_N, freeN, ours,
                   kDropRates[g_dropLevel].minFree + 1, (int)*(volatile DWORD *)0xFD5A90, (int)*(volatile DWORD *)0xFD5A94, t);
}

static void LhFeatSampleFrame(void)  // a play frame
{
    int f;
    for (f = 0; f < LHF_N; ++f) {
        if (!LhFeatOn(f)) continue;
        ++g_lhF[f].onF;
        switch (f) {
        case LHF_ECHO: if (g_echoDrawN > 0) ++g_lhF[f].actF; break;
        case LHF_AI: if (g_aiFrameStamp == g_fxFrame && g_aiEchoN > 0) ++g_lhF[f].actF; break;
        case LHF_DP: if (g_dpArtN > 0) ++g_lhF[f].actF; break;
        case LHF_NC: if (g_ncLastOutOk) ++g_lhF[f].actF; break;
        case LHF_COPIES: g_lhF[f].cMax = g_enmKillsKept - g_lhF[f].c0 > g_lhF[f].cMax ? g_enmKillsKept - g_lhF[f].c0 : g_lhF[f].cMax; break;
        case LHF_FOLLOW: if (g_enmQN > g_lhF[f].cMax) g_lhF[f].cMax = g_enmQN; break;
        case LHF_DROPS: {
            const LONG d = (LONG)(LhLedgerSum() - g_lhLedger0) + (LONG)(g_dropCopyN - g_lhDropCopy0);
            if (d > g_lhF[f].cMax) g_lhF[f].cMax = d;
            break;
        }
        }
    }
    if (LhFeatOn(LHF_AI) && g_aiStunsRoom > g_lhF[LHF_AI].cMax) g_lhF[LHF_AI].cMax = g_aiStunsRoom;
    {   // her walk (horizontal), for After Shock's "she moved, so a trail must show"
        const float *p = (const float *)0x10A0F60;
        if (g_lhPosOk) {
            const float dx = p[0] - g_lhPos[0], dz = p[2] - g_lhPos[2], d2 = dx * dx + dz * dz;
            if (d2 < 200.0f * 200.0f) g_lhWalk += (float)sqrt(d2);  // a bigger jump is a warp, not a walk
        }
        g_lhPos[0] = p[0], g_lhPos[1] = p[1], g_lhPos[2] = p[2];
        g_lhPosOk = _finite(p[0]) && _finite(p[2]);
    }
}

// ---------------------------------------------------------------- the room: begin / end
static int LhRoomSeen(DWORD key)
{
    int i;
    for (i = 0; i < g_lhRoomKinds && i < 64; ++i)
        if (g_lhSeen[i] == key) return 1;
    if (g_lhRoomKinds < 64) g_lhSeen[g_lhRoomKinds] = key;
    ++g_lhRoomKinds;
    return 0;
}

static void LhRoomBegin(DWORD key, LARGE_INTEGER now)
{
    g_lhRoomKey = key;
    g_lhRoomQ = now;
    g_lhMsPlay = g_lhMsCut = g_lhMsPause = 0.0;
    LhStatsReset(&g_lhRoom);
    g_lhWorstAt = g_lhWorstOwn = 0.0f;
    g_lhWorstWhy[0] = 0;
    g_lhWorstLoad = 0;  // v3.35 FX335
    memset(&g_lhFreezeRate, 0, sizeof g_lhFreezeRate);
    memset(g_lhF, 0, sizeof g_lhF);
    g_lhF[LHF_COPIES].c0 = g_enmKillsKept;
    g_lhKill0 = *(volatile LONG *)0xFD5BD8;
    g_lhCopyKill0 = g_enmKillsKept;
    g_lhLedger0 = LhLedgerSum();
    g_lhDropCopy0 = g_dropCopyN;
    g_lhWalk = 0.0f;
    g_lhPosOk = 0;
    g_lhQsFlipsRoom = 0;
    LhRoomSeen(key);
}

static void LhVaPoll(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    if (!GlobalMemoryStatusEx(&ms)) return;
    g_lhVaTotal = (int)(ms.ullTotalVirtual >> 20);
    g_lhVaUsed = (int)((ms.ullTotalVirtual - ms.ullAvailVirtual) >> 20);
    if (g_lhVaUsed > g_lhVaPeak) g_lhVaPeak = g_lhVaUsed;
    if (LhVaStep(&g_lhVaLow, g_lhVaUsed, g_lhVaTotal))
        LogAnomaly("memory", "address space %d of %d MB in use: %d MB left before the 32-bit limit (the exe is not large-"
                             "address aware; an allocation failure crashes the game); stage %X room %X",
                   g_lhVaUsed, g_lhVaTotal, g_lhVaTotal - g_lhVaUsed, g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
}

static void LhIniPoll(const char *when)  // menu values that could not be saved (the features' own flags)
{
    int k, bad = 0;
    char keys[200];
    int o = 0;
    keys[0] = 0;
    for (k = 0; k < GP_N; ++k)
        if (g_gpUnsaved[k] && kGpIniKey[k]) {
            ++bad;
            if (o < (int)sizeof keys - 40) o += sprintf_s(keys + o, sizeof keys - o, "%s%s", o ? ", " : "", kGpIniKey[k]);
        }
    for (k = 0; k < PC_COUNT; ++k)
        if (g_pcUnsaved[k]) {
            ++bad;
            if (o < (int)sizeof keys - 40) o += sprintf_s(keys + o, sizeof keys - o, "%s%s", o ? ", " : "", kPcIniKey[k]);
        }
    for (k = 0; k < HB_K_N; ++k)
        if (g_hbUnsaved[k]) {
            ++bad;
            if (o < (int)sizeof keys - 40) o += sprintf_s(keys + o, sizeof keys - o, "%s%s", o ? ", " : "", kHbIniKey[k]);
        }
    if (g_ncUnsaved) {
        ++bad;
        if (o < (int)sizeof keys - 40) o += sprintf_s(keys + o, sizeof keys - o, "%sNoClip", o ? ", " : "");
    }
    if (bad > g_lhUnsavedSeen) {
        g_lhUnsavedSeen = bad;
        LogAnomaly("ini", "%d menu value(s) could not be written to SH4Refined.ini [Gameplay] (%s; %s): they apply now but "
                          "are lost at the next start", bad, keys, when);
    }
}

static void LhFaultPoll(void)
{
    static const char *const kName[8] = { "Echo Doubles", "After Shock", "Death Presence", "Enemy Copies / Follow / Revive",
                                          "Supply Drops", "the downed-ghost pickup", "No Clip", "this log" };
    int now[8], i;
    now[0] = g_echoFail, now[1] = g_aiFail, now[2] = g_dpFail, now[3] = g_enmFail;
    now[4] = g_dropFail, now[5] = g_pkFail, now[6] = g_ncFault, now[7] = g_lhFail;
    for (i = 0; i < 8; ++i)
        if (now[i] && !g_lhFaultSeen[i]) {
            g_lhFaultSeen[i] = 1;
            LogAnomaly("fault", "%s faulted and is off for the rest of the session (stage %X room %X; its own 'faulted' "
                                "line above names the code and the address)", kName[i], g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
        }
    if (g_echoDrawFaults > g_lhEchoDrawFaults) {
        LogAnomaly("fault", "Echo Doubles' draw faulted %d time(s) so far this session (stage %X room %X)", g_echoDrawFaults,
                   g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
        g_lhEchoDrawFaults = g_echoDrawFaults;
    }
}

static void LhRoomEnd(LARGE_INTEGER now)
{
    const DWORD key = g_lhRoomKey;
    const int stage = (int)(key >> 16), r302 = stage == 1 || stage == 0x0A;
    const float secs = LhMs(now.QuadPart - g_lhRoomQ.QuadPart) / 1000.0f, play = (float)(g_lhMsPlay / 1000.0);
    const LONG kills = *(volatile LONG *)0xFD5BD8 - g_lhKill0, copyKills = g_enmKillsKept - g_lhCopyKill0;
    char feats[900], v[48];
    int f, o = 0;
    ++g_lhRooms;
    LhStatsMerge(&g_lhSess, &g_lhRoom);
    if (g_lhRoom.worst > 0.0f && g_lhRoom.worst >= g_lhSess.worst) {
        g_lhSessWorstKey = key;
        g_lhSessWorstAt = g_lhWorstAt;
        strcpy_s(g_lhSessWorstWhy, sizeof g_lhSessWorstWhy, g_lhWorstWhy);
    }
    LhVaPoll();
    if (!g_lhRoom.n)
        LogV("room: %X:%02X left after %.1f s (play %.1f, cutscene %.1f, paused %.1f); no play frames; stalls %u; VA %d of %d MB "
             "(margin %d)", stage, (int)(key & 0xFFFF), secs, play, g_lhMsCut / 1000.0, g_lhMsPause / 1000.0, g_lhRoom.stalls,
             g_lhVaUsed, g_lhVaTotal, g_lhVaTotal - g_lhVaUsed);
    else
    LogV("room: %X:%02X left after %.1f s (play %.1f, cutscene %.1f, paused %.1f); %u frames: mean %.1f ms, p99 %.1f, worst "
         "%.1f at +%.1f s (%s%sRefined %.2f ms%s%s), spikes %u (over %.0f ms), stalls %u; VA %d of %d MB (margin %d)",
         stage, (int)(key & 0xFFFF), secs, play, g_lhMsCut / 1000.0, g_lhMsPause / 1000.0, g_lhRoom.n, LhStatsMean(&g_lhRoom),
         LhStatsPct(&g_lhRoom, 99.0f), g_lhRoom.worst, g_lhWorstAt, g_lhWorstAt * 1000.0f < LH_ENTRY_MS ? "entry" : g_lhWorstLoad ? "after a load" : "in play",
         "; ", g_lhWorstOwn, g_lhWorstWhy[0] ? "; " : "", g_lhWorstWhy, g_lhRoom.spikes,
         LhSpikeMs(*(volatile int *)0x5D5248 > 0 ? 1000.0f / (float)*(volatile int *)0x5D5248 : 0.0f), g_lhRoom.stalls,
         g_lhVaUsed, g_lhVaTotal, g_lhVaTotal - g_lhVaUsed);
    if (LhStutter(g_lhRoom.spikes, play))
        LogAnomaly("frame", "stutter in stage %X room %X: %u spikes in %.0f s of play (p99 %.1f ms, worst %.1f ms: %s)", stage,
                   (int)(key & 0xFFFF), g_lhRoom.spikes, play, LhStatsPct(&g_lhRoom, 99.0f), g_lhRoom.worst,
                   g_lhWorstWhy[0] ? g_lhWorstWhy : "no Refined timer rose: the game, the driver or the disk");
    feats[0] = 0;
    for (f = 0; f < LHF_N; ++f) {
        LONG acted;
        if (!g_lhF[f].onF) continue;
        acted = f <= LHF_NC ? g_lhF[f].actF : g_lhF[f].cMax;
        LhFeatValue(f, v, sizeof v);
        ++g_lhFRoomsOn[f];
        if (acted) ++g_lhFRoomsAct[f];
        g_lhFTotal[f] += acted;
        if (o < (int)sizeof feats - 160) {
            o += sprintf_s(feats + o, sizeof feats - o, "%s%s=%s acted %ld %s", o ? "; " : "", kLhFeat[f], v, acted, kLhActUnit[f]);
            if (f == LHF_AI) o += sprintf_s(feats + o, sizeof feats - o, " (stuns %ld, walked %.0f u)", g_lhF[f].cMax, g_lhWalk);
            if (f == LHF_DROPS) {
                int budget, here = LhLedgerHere(&budget), ours, freeN = LhPickupFree(&ours);
                o += sprintf_s(feats + o, sizeof feats - o, " (kills %ld + copies %ld, ledger here %d%s%.0d, pickup records free "  // v3.35 MERGE
                               "%d of %d)", kills, copyKills, here, budget ? " of " : ", no budget", budget, freeN, DROP_REC_N);
            }
        }
    }
    if (g_lhQsFlipsRoom && o < (int)sizeof feats - 60)
        o += sprintf_s(feats + o, sizeof feats - o, "%sQuick Save row flips %d", o ? "; " : "", g_lhQsFlipsRoom);
    if (o) LogV("room: %X:%02X features: %s", stage, (int)(key & 0xFFFF), feats);
    BgRoomLog(stage, (int)(key & 0xFFFF), *(volatile int *)0x5D5248 > 0 ? 1000.0f / (float)*(volatile int *)0x5D5248 : 0.0f);  // v3.35 FX335
    // the decidable idle checks
    {
        const int eligE = !r302 && play >= LH_IDLE_PLAY_S;
        if (LhIdleStep(&g_lhIdle[LHF_ECHO], g_lhF[LHF_ECHO].onF > 0, eligE, g_lhF[LHF_ECHO].actF > 0, LH_IDLE_ROOMS))
            LogAnomaly("echo", "Echo Doubles on for %d rooms in a row (each %.0f s or more of play, not Room 302) and no shadow "
                               "was drawn in any: the shadows never formed or their draw is not reached (last room %X:%02X)",
                       LH_IDLE_ROOMS, LH_IDLE_PLAY_S, stage, (int)(key & 0xFFFF));
        if (LhIdleStep(&g_lhIdle[LHF_AI], g_lhF[LHF_AI].onF > 0, !r302 && g_lhWalk >= LH_WALK_U, g_lhF[LHF_AI].actF > 0,
                       LH_IDLE_ROOMS))
            LogAnomaly("after shock", "After Shock on for %d rooms in a row in which she walked %.0f units or more (not Room "
                                      "302) and no trail copy was drawn in any (last room %X:%02X, walked %.0f u)",
                       LH_IDLE_ROOMS, LH_WALK_U, stage, (int)(key & 0xFFFF), g_lhWalk);
        if (g_lhF[LHF_DROPS].onF) {
            int budget, here = LhLedgerHere(&budget);
            if (g_lhF[LHF_DROPS].cMax > 0 || here >= budget) {
                g_lhDropKills = 0;  // a drop came, or the world's budget is used up (no drop is due)
                g_lhDropFlagged = 0;
            } else if (!r302) {
                g_lhDropKills += (int)(kills > 0 && kills < 50 ? kills : 0) + (int)(copyKills > 0 && copyKills < 50 ? copyKills : 0);
                if (!g_lhDropFlagged && g_dropLevel > 0 && g_dropLevel < DROP_LEVELS &&
                    LhDropsSuspicious(g_lhDropKills, 0, kDropRates[g_dropLevel].common)) {
                    g_lhDropFlagged = 1;
                    LogAnomaly("supply drops", "%d monster kills since the last drop at %s (each at least %.0f %%: under 1 %% "
                                               "chance of none) and no drop; this world visit's ledger %d of %d (stage %X): no "
                                               "'supply drops:' roll lines = the kills were not seen; 'supply drops: no drop:' "
                                               "lines name each cause (roll, budget, pool, model)",  // v3.33 MERGE: W6's wording
                               g_lhDropKills, LhGpName(GP_DROP, g_dropLevel), kDropRates[g_dropLevel].common * 100.0f, here,
                               budget, stage);
                }
            }
        }
    }
    LhIniPoll("seen at a room change");
}

// ---------------------------------------------------------------- the Quick Save row while one pause menu is open
static void LhQsSample(DWORD tick)
{
    // v3.33 MERGE: the look W5's EmQsRows shows (held QS_HOLD_MS) while the sub-list is open; its reason = pm_why_qs
    const int ok = g_emQsOpen ? (g_emQsAv[0].shown ? 1 : 0) : (pm_can_qs() ? 1 : 0);
    const char *why = NULL;
    if (!ok) {
        why = pm_why_qs();
        if (!why) why = g_emQsWhy[0] ? g_emQsWhy[0] : "held grey (its reason just passed)";
    }
    if (!g_lhInPause) {
        g_lhInPause = 1;
        g_lhPauseTick = tick;
        LhFlipReset(&g_lhQsFlip);
        g_lhQsWhy[0] = 0;
        g_lhQsAnom = 0;
    }
    if (LhFlipStep(&g_lhQsFlip, ok)) {
        ++g_lhQsFlipsRoom;
        if (LogQuota(&g_lhQsQ, 80))
            LogV("extra menu: the Quick Save row turned %s after %.1f s of this pause%s%s (stage %X room %X)",
                 ok ? "available" : "grey", (tick - g_lhPauseTick) / 1000.0f, why ? ": " : "", why ? why : "",
                 g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
        if (why && strlen(g_lhQsWhy) + strlen(why) + 4 < sizeof g_lhQsWhy && !strstr(g_lhQsWhy, why)) {
            if (g_lhQsWhy[0]) strcat_s(g_lhQsWhy, sizeof g_lhQsWhy, "; ");
            strcat_s(g_lhQsWhy, sizeof g_lhQsWhy, why);
        }
        if (g_lhQsFlip.flips >= 4 && !g_lhQsAnom) {
            g_lhQsAnom = 1;
            LogAnomaly("extra menu", "the Quick Save row changed between grey and available %d times in %.1f s of one pause "
                                     "(it should hold still while paused; grey because: %s; stage %X room %X)",
                       g_lhQsFlip.flips, (tick - g_lhPauseTick) / 1000.0f, g_lhQsWhy[0] ? g_lhQsWhy : "?",
                       g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
        }
    } else if (g_lhQsFlip.flips == 0 && !ok && why && !g_lhQsWhy[0]) {
        strcpy_s(g_lhQsWhy, sizeof g_lhQsWhy, why);  // grey from the start: say why once at the end of the pause
    }
}
static void LhQsPauseEnd(DWORD tick)
{
    if (!g_lhInPause) return;
    g_lhInPause = 0;
    if (g_lhQsFlip.have && !g_lhQsFlip.last && g_lhQsFlip.flips == 0 && LogQuota(&g_lhQsQ, 80))
        LogV("extra menu: the Quick Save row stayed grey through a %.1f s pause (%s; stage %X room %X)",
             (tick - g_lhPauseTick) / 1000.0f, g_lhQsWhy[0] ? g_lhQsWhy : "?", g_lhRoomKey >> 16, g_lhRoomKey & 0xFFFF);
}

// ---------------------------------------------------------------- the frame
// v3.35 FX335: a frame's context from the ledger: its top costs (at least 0.3 ms each) and the effect budget's level
static void LhWhy(const float led[BGC_N], char *out, size_t n)
{
    int o;
    BgTopText(led, 3, 0.3f, out, n);
    o = (int)strlen(out);
    if (g_bgOn && (size_t)o + 24 < n) sprintf_s(out + o, n - (size_t)o, "%sbudget level %d", o ? "; " : "", BgLevel());
}
static void LhFrameEndC(void)
{
    LARGE_INTEGER now;
    const int phase = *(volatile int *)0xFD5C20;
    const DWORD key = *(volatile DWORD *)0xFD5A90 << 16 | (*(volatile DWORD *)0xFD5A94 & 0xFFFF), tick = GetTickCount();
    float dt, own, led[BGC_N], sinceLoad = 1.0e9f;
    QueryPerformanceCounter(&now);
    if (!g_lhQf.QuadPart) QueryPerformanceFrequency(&g_lhQf);
    own = LhMs(now.QuadPart - g_lhBeginQ.QuadPart);
    BgFrameTake(led);  // v3.35 FX335: this frame's parts (budget.inc); "Refined" = their total (the draw and the tick)
    {
        const float tot = BgTotalMs(led);
        if (tot > 0.0f) own = tot;
    }
    g_lhOwnMs = own;  // v3.37 CP337: perfhud.inc's "Refined" line
    InterlockedIncrement(&g_lhOwnN);
    if (!g_lhFirstFrameDone) {
        g_lhFirstFrameDone = 1;
        LhFirstFrame();
        LhVaPoll();
    }
    if (g_lhVeh) {  // v3.59 SD359: the faults noted since the last frame; the Direct3D stop; late driver images
        LhFaultDrain();
        if (g_lhD3dTrip && !g_lhD3dStopped) {
            g_lhD3dStopped = 1;
            LhD3dStop();
        }
        if (tick - g_lhGfxTick >= 60000) g_lhGfxTick = tick, LhGfxScan();
    }
    LhLoadCheck();
    if (key != g_lhRoomKey) {
        if (g_lhRoomKey != 0xFFFFFFFFu) LhRoomEnd(now);
        LhRoomBegin(key, now);
        g_lhPrevQ = now;
        g_lhPrevPhase = phase;
        g_lhPlayRun = 0;  // v3.35 FX335
        BgFrame(0.0f, 0.0f, 0.0f, 1, 0, key);
        g_lhPrevEchoMs = g_echoMsMax, g_lhPrevAiMs = g_aiMsMax, g_lhPrevDfxMs = g_dfxMsMax, g_lhPrevDpMs = g_dpMsMax;
        g_lhPrevEchoTick = g_echoTickMs;
        return;  // this frame's interval is the room change itself (loading)
    }
    dt = LhMs(now.QuadPart - g_lhPrevQ.QuadPart);
    if (g_lhLoadSeen) sinceLoad = LhMs(now.QuadPart - g_lhLoadAtQ.QuadPart);  // v3.35 FX335
    if (g_lhPrevPhase == 0) g_lhMsPlay += dt;
    else if (g_lhPrevPhase == 1) g_lhMsCut += dt;
    else g_lhMsPause += dt;
    if (phase == 0 && g_lhPrevPhase == 0 && !g_qsLoading) {
        const int rate = *(volatile int *)0x5D5248;
        const float target = rate >= 10 && rate <= 480 ? 1000.0f / (float)rate : 0.0f;
        const float worstBefore = g_lhRoom.worst;
        const float since = LhMs(now.QuadPart - g_lhRoomQ.QuadPart);
        const int c = LhStatsAdd(&g_lhRoom, dt, target);
        char why[128];
        why[0] = 0;
        if (g_logLevel >= 3) LhStatsAdd(&g_lhTrace, dt, target);
        if (c == LHF_SPIKE || (c != LHF_SKIP && c != LHF_STALL && dt > worstBefore)) LhWhy(led, why, sizeof why);  // v3.35 FX335
        if (c != LHF_SKIP && c != LHF_STALL && dt > worstBefore) {  // the room's worst so far: keep its context
            // v3.35 FX335: this frame's own top costs (budget.inc's ledger); v3.33 named a feature only when its room maximum
            // rose, so most slow frames named none
            strcpy_s(g_lhWorstWhy, sizeof g_lhWorstWhy, why);
            g_lhWorstAt = since / 1000.0f;
            g_lhWorstOwn = own;
            g_lhWorstLoad = sinceLoad < LH_ENTRY_MS;
        }
        if (c == LHF_SPIKE && LhFreezeIsAnomaly(dt, since < sinceLoad ? since : sinceLoad) && LhRateOk(&g_lhFreezeRate, tick, 0xFFFFFFFFu, 2) &&
            LogQuota(&g_lhFreezeQ, 30))  // v3.35 FX335: not within 3 s of a load either (the user's 276 ms "freeze in play" was a load's frame)
            LogAnomaly("frame", "a %.0f ms freeze in play, stage %X room %X, %.1f s into the room (target %.1f ms; Refined's "
                                "per-frame work %.2f ms of it%s%s)", dt, key >> 16, key & 0xFFFF, since / 1000.0f, target, own,
                       why[0] ? "; " : "", why);
        LhFeatSampleFrame();
        ++g_lhPlayRun;
        // v3.35 FX335: the effect budget: not the room's or a load's first 1.5 s, a warm-up frame, or the first 20 frames after
        // a pause / cut-scene (their first frames are not the effects' doing)
        {   // v3.56 MERGE: preview.inc's pause capture drew in this interval: not the effects' doing (a skip frame)
            static LONG s_pvSeen;
            const int pvCap = PvCapNew(&s_pvSeen);
            g_bgPwMask = BgPowerMask(led[BGC_ECHO], led[BGC_AI], led[BGC_DP], g_gtFade.phase != GTF_IDLE);  // v3.58 GX358: the powers drawing (the start step)
            BgFrame(dt, target, BgEffectsMs(led), since < 1500.0f || sinceLoad < 1500.0f || led[BGC_WARM] > 0.5f || g_lhPlayRun < 20 || pvCap, 1, key);
        }
    } else {
        g_lhPlayRun = 0;  // v3.35 FX335
        BgFrame(dt, 0.0f, 0.0f, 1, 0, key);
    }
    g_lhPrevEchoMs = g_echoMsMax, g_lhPrevAiMs = g_aiMsMax, g_lhPrevDfxMs = g_dfxMsMax, g_lhPrevDpMs = g_dpMsMax;
    g_lhPrevEchoTick = g_echoTickMs;
    // the pause menu: the Extra Menu's Quick Save row should hold still
    if (g_extraTick && g_emOn[EM_QSAVE] && *(volatile int *)0x107BF54 == 2 && !*(volatile int *)0x107BF58) LhQsSample(tick);
    else LhQsPauseEnd(tick);
    if (tick - g_lhFaultTick >= 1000) {
        g_lhFaultTick = tick;
        LhFaultPoll();
        if (g_logLevel >= 3 && g_lhTrace.n + g_lhTrace.stalls) {
            LogT("frame: 1 s: %u frames, mean %.2f ms, max %.2f, spikes %u, stalls %u, phase %d, stage %X room %X", g_lhTrace.n,
                 LhStatsMean(&g_lhTrace), g_lhTrace.worst, g_lhTrace.spikes, g_lhTrace.stalls, phase, key >> 16, key & 0xFFFF);
            LhStatsReset(&g_lhTrace);
        }
    }
    if (tick - g_lhVaTick >= 10000) {
        g_lhVaTick = tick;
        LhVaPoll();
    }
    g_lhPrevQ = now;
    g_lhPrevPhase = phase;
}

static void LhFrameBegin(void)
{
    QueryPerformanceCounter(&g_lhBeginQ);
}

static void LhFrameEnd(void)
{
    if (g_lhFail || !g_lhHeaderDone || InterlockedCompareExchange(&g_lhEnded, 0, 0)) return;
    __try {
        LARGE_INTEGER a, b;  // v3.35 FX335: this file's own time, in the next frame's ledger
        QueryPerformanceCounter(&a);
        LhFrameEndC();
        QueryPerformanceCounter(&b);
        g_bgAcc[BGC_LOG] += b.QuadPart - a.QuadPart;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_lhFail = 1;
        g_bgOn = 0;  // v3.35 FX335: the effect budget is fed from here: off with it (everything full, never held thinned)
        InterlockedExchange(&g_bgLevel, 0);
        Log("log: the health log's frame check faulted (code %08lX): room lines and framework checks are off for this "
            "session (and the effect budget: everything drawn full)", GetExceptionCode());
    }
}

// ---------------------------------------------------------------- the session summary (exit or crash)
static void LhSessionEndC(const char *why)
{
    LARGE_INTEGER now;
    char feats[900];
    int f, o = 0, faults = 0, i;
    unsigned secs;
    QueryPerformanceCounter(&now);
    if (g_lhVeh) LhFaultDrain();  // v3.59 SD359: the faults not logged yet (a crash's own comes in the crash line)
    if (g_lhRoomKey != 0xFFFFFFFFu && !g_lhFail) {
        LhRoomEnd(now);
        g_lhRoomKey = 0xFFFFFFFFu;
    }
    secs = (unsigned)(LhMs(now.QuadPart - g_lhSessQ.QuadPart) / 1000.0f);
    for (i = 0; i < 8; ++i) faults += g_lhFaultSeen[i];
    if (!g_lhSess.n)
        Log("session: end (%s) after %u:%02u:%02u: %d room visit(s), no play frames; %ld ANOMALY line(s); %d feature(s) "
            "faulted; ini %s", why, secs / 3600, secs / 60 % 60, secs % 60, g_lhRooms, g_logAnomalies, faults,
            g_lhIniBad || g_lhUnsavedSeen ? "NOT fully written" : "fine");
    else
    Log("session: end (%s) after %u:%02u:%02u: %d room visit(s) (%d rooms); frames %u: mean %.1f ms, p99 %.1f, worst %.1f in "
        "%X:%02X at +%.1f s%s%s, spikes %u, stalls %u; VA peak %d of %d MB (margin %d); %ld ANOMALY line(s); %d feature(s) "
        "faulted; ini %s", why, secs / 3600, secs / 60 % 60, secs % 60, g_lhRooms, g_lhRoomKinds, g_lhSess.n,
        LhStatsMean(&g_lhSess), LhStatsPct(&g_lhSess, 99.0f), g_lhSess.worst, g_lhSessWorstKey >> 16,
        (int)(g_lhSessWorstKey & 0xFFFF), g_lhSessWorstAt, g_lhSessWorstWhy[0] ? " (" : "", g_lhSessWorstWhy[0] ? g_lhSessWorstWhy : "",
        g_lhSess.spikes, g_lhSess.stalls, g_lhVaPeak, g_lhVaTotal, g_lhVaTotal - g_lhVaPeak, g_logAnomalies, faults,
        g_lhIniBad || g_lhUnsavedSeen ? "NOT fully written" : "fine");
    if (g_lhSessWorstWhy[0]) Log("session: (the worst frame's own timers: %s)", g_lhSessWorstWhy);
    feats[0] = 0;
    for (f = 0; f < LHF_N; ++f) {
        if (!g_lhFRoomsOn[f] || o >= (int)sizeof feats - 120) continue;
        o += sprintf_s(feats + o, sizeof feats - o, "%s%s acted in %d of %d room(s) on (%ld %s)", o ? "; " : "", kLhFeat[f],
                       g_lhFRoomsAct[f], g_lhFRoomsOn[f], g_lhFTotal[f], kLhActUnit[f]);
    }
    Log("session: features: %s", o ? feats : "none on in a room");
    LhFaultEndLine();  // v3.59 SD359
    RzSessionLine();  // v3.48 RZ348: the Randomizer's session line (randomizer.inc)
}

static void LhSessionEnd(const char *why)
{
    if (!g_log || !g_lhHeaderDone || InterlockedExchange(&g_lhEnded, 1)) return;
    // at process exit other threads are gone, maybe holding the log's lock: write only when it is free
    if (g_logCsInit == 2 && !TryEnterCriticalSection(&g_logCs)) return;
    __try {
        LhSessionEndC(why);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    if (g_logCsInit == 2) LeaveCriticalSection(&g_logCs);
}
