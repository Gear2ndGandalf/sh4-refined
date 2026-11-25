// SH4 Refined (formerly SH4 Window Mod): windowed mode, resizing, no freeze / minimize on Alt+Tab and mouse capture for
// SILENT HILL 4: The Room (GOG v7), as a winmm.dll proxy in the game folder.
//
// Split out of the Playable Eileen plugin (dsound.dll, plugin/src/window.c, v0.29-v0.81) on 2026-10-26 so the
// window features are a mod of their own; merged with the laptop rig's SH4Windowed.asi (src/sh4windowed, 2026-10-20):
//   * windowed mode (4:3 client, centred, never topmost), resizing with 4:3 snapping, the game and the GOG wrappers
//     always see their window as active (no minimize / pause on focus loss), CaptureMouse (hidden + confined while
//     the game is in front, released on Alt+Tab / focus loss / drag), DirectInput non-exclusive - from window.c;
//   * per-vtable originals for the DirectInput hooks (controller-connect crash, 2026-10-19) and the GOG dinput8.dll
//     HID read bounce buffer (DualSense crash, hidfix2 2026-10-20) - from SH4Windowed.asi;
//   * the exe fix no_minimize (0x415040 74 26 -> EB 26), applied in memory, from the plugin's ApplyExeFixes.
// v1.2 (2026-10-29, laptop rig): [Window] FourByThree. The GOG exe's Options screen-size list (0x4148e0, index
// [0x610dd4], sizes written to [0x5d524c] / [0x5d5250]) is 640x480, 1024x768, 1366x768, 1920x1080, 2560x1440,
// 3840x2160, and the game builds its 3D projection from that size, while GOG's wrapper presents the picture in the
// 4:3 window / display= size: a wide size gives a wide-angle picture squeezed into 4:3 (tall and narrow). The four
// wide sizes are rewritten in memory to 4:3 sizes the display supports (EnumDisplaySettings: the smallest 4:3 mode
// at least as tall, else the largest; a size that is no display mode came out as a 1600x1440 picture with side
// bars); the Options menu still names them by their old labels. The wrapper's aspect correction (dxcfg.ini
// aspect=enabled) fits the picture to the SCREEN's shape, so in a 4:3 window it drew side bars (4:3 of 16:10 = 83 %
// of the width); its screen-size queries (GetSystemMetrics, GetDeviceCaps, EnumDisplaySettingsA current mode, IAT of
// the wrapper modules) now get the game window's client size, so the correction fits the window: a 4:3 window has
// no bars, a maximised window gets real side bars. [Window] WideFullScreen: while the window is maximised (or its
// client covers the monitor) the wide entries are switched to a wide size of the screen's shape inside the display=
// cap and the game re-applies its size itself (the main tick 0x41507d calls 0x4148e0 every frame, which re-applies
// the Options entry whenever [0x610dd8] != [0x610dd4]: the mod writes -1 there), 4:3 again on restore.
// v1.3 (2026-10-29, laptop rig) replaces FourByThree / WideFullScreen and the wrapper screen-size hooks (research:
// src\research\options_menu, gog_wrapper, projection): [Window] AspectMenu adds an "Aspect Ratio" row (4:3 / 16:10 /
// 16:9) under Resolution in Options > Option 1; Resolution keeps its six stock entries and shows the six sizes of the
// chosen shape (480p .. 4K); the shape is saved in the Options block's unused byte cfg+0x21, so sh4.sav stays valid
// for the stock game. The 3D projection follows the real game size (GOG keyed it on the Options index: a 4:3 size
// in a "wide" entry squeezed the world while the menus looked right), Hor+ for wide shapes. The window keeps its
// height and takes the picture's shape; the game's mouse position is scaled from the window to the game size.
// [Window] FitWrapper: GOG's wrapper reads dxcfg.ini display= as "application" (its swap chain = the game size; a
// display= value without @Hz always meant the desktop) and presentation= as "windowed".
// v1.4 (same evening): [Window] Menus43, menus / text / HUD / movies stay 4:3 in wide shapes (section "v1.4" below).
// v1.6 (2026-11-01, main rig): full-screen darkening / black fills cover the whole wide picture (the item examine
// screen's darkening 50EE30 and every other untextured full-canvas sprite or batch; pictures and the rest of the GUI
// stay 4:3), [Window] Menus43Log (diagnostic), and the ultrawide shapes 21:9 and 32:9 in the Aspect Ratio row.
// v1.7 (2026-11-01, main rig): a Monitor row in Options (with two or more screens; section "v1.7: monitor" below): the
// window, Borderless and the D3D device go to the chosen screen, which is remembered in [Window] Monitor /
// MonitorName / MonitorPath (also when the window is dragged to another screen). The Display Mode row's right arrow
// no longer moves between Windowed and Borderless.
// v1.8 (2026-11-02, main rig): [Window] MenuPillars, black sides beside full-screen 4:3 pictures in wide shapes (the
// memo reader, the Scrap Book, the item box, the map, puzzle close-ups, the game over and results screens): every
// opaque (alpha blending off), textured, unrotated sprite that covers the whole canvas gets the sides cleared to black
// first, so the 3D world no longer shows beside it (section "v1.6: full-screen fills" below).
// v1.9 (2026-11-03, laptop rig): a "Field of View" row under Aspect Ratio (Default, x0.8 .. x1.5; [Window] FieldOfView
// in percent). It scales the gameplay field of view where the game sets it (0x4F70A0, section "v1.9: field of view"
// below); cutscenes keep theirs. And an EXPERIMENTAL "Frame Rate" row (30 / 60 / 120 / 180; [Window] FrameRate, applied at
// the next start): the engine's own rate variable [0x5D5248] and the main loop's timer / animation rate are set to it
// (section "v1.9: frame rate"; research\framerate\report.md). Some per-tick values are still 30 Hz ones.
// v2.0 (2026-11-03, laptop rig, user: FOV "from 60 to 120 in increments of 5", "FOV ... does not immediately change in
// Room 302", "the title menu is very fast", "frame rate does not change during gameplay immediately"):
// * Field of View in degrees ([Window] FOV, 0 = Default; v1.9's FieldOfView percent is converted once): the horizontal
//   angle of the usual third-person camera at the picture's shape; a change applies at once, in Room 302 too.
// * Frame rate engine (section FRAME RATE ENGINE): the front end and the in-game menus run at 30 as shipped, gameplay at
//   the chosen rate, switched at frame boundaries, so a change in the menu applies when play resumes; animations scaled
//   at their three advance sites; 164 per-tick constants of the exe follow the rate (research fps_tail.md); full
//   screen with VSync is capped at the refresh rate.
// * Alt+Enter in Options also updates the Options backup (answering "No" no longer switches the display mode back).
// v2.1 PROTOTYPE (2026-11-04, interpolation branch): [Window] FrameMode. 1 (default): the logic stays at 30 Hz and the
// Frame Rate row picks the DISPLAY rate: every frame is recorded at the device and shown again with the camera / object /
// bone constants blended towards it (FRAME INTERPOLATION, interp.c / interp.h; research\framerate\v3\interpolation.md).
// 0: the v2.0 frame rate engine (the logic runs at the chosen rate).
// v3.3.0 (2026-11-09, main rig): FOV First Person / FOV Third Person (the user: "Split Field of View option into FOV
// first person and FOV third person"; [Window] FOVFirstPerson + FOV, the setter's caller picks the row), the
// [Textures] Pack hook (texpack.inc, off by default) and [Items] Sparkle (items.inc).
// v3.3.5 (2026-11-10, main rig): Item Sparkle is called Item Shine ([Items] Shine*; the old Sparkle* keys are converted).
// The shine keeps going through pickups and stays, frozen, in the paused picture; companions (Eileen, Cynthia) hide
// it; FOV First Person offers Default and 100-120; Room 302's window and peephole views keep the game's own angle.
// v3.4.0 (2026-11-10, main rig): [Textures] Pool (default 0 = v3.3.5). Pool=1 makes the replacement textures in
// D3DPOOL_DEFAULT (staging + UpdateTexture), so no system-memory copy stays in the 2 GB address space; the game's device
// reset 0x402CD0 is hooked at 0x402E17 / 0x402E2E to keep and refill them (texpack.inc, staging\texpool\REPORT.md).
// v3.5.0 (2026-11-14, main rig, session 9b): Options 1-3 are the stock pages again (Resolution keeps its sizes per
// shape); new virtual pages Option 4 (Presentation: Display Mode, Monitor, Aspect Ratio, 1st / 3rd Person FOV, Frame
// Rate) and Option 5 (Gameplay: Item Shine); Option 6 (Extra Menus) is defined, not yet in the cycle
// (staging\options_pages\PLAN.md, claudex-loop).
// v3.5.1 (2026-11-14): changed rows are coloured per page (the row draw's 523AA0 call); 1st / 3rd Person FOV, Frame
// Rate and Item Shine are compared with a snapshot taken when Options opens: yellow when changed, the exit asks, and
// "No" puts them back (hook on the discard's `call 524320` at 0x526B8D).
// v3.5.2 (2026-11-14): a save made in a world is titled after that world ([Saves] WorldTitles; hook 0x56F76F,
// staging\save_names\PLAN.md).
// v3.6.0 (2026-11-14): Quick Save F5 / Quick Load F9 with 3 rotating slots in save\quick, kept after a restart, with
// soft-lock guards (quicksave.inc; staging\quick_save\PLAN.md).
//
// Loading: the exe imports WINMM.dll (timeBeginPeriod, timeEndPeriod, timeGetTime, timeKillEvent, timeSetEvent) and
// winmm is not a KnownDLL, so Windows takes winmm.dll from the game folder. Every export of the 32-bit system winmm.dll
// is forwarded (winmm_exports.inc, generated by tools/gen_exports.py); the real dll is loaded on first use from
// %windir%\SysWOW64 by full path, never from the game folder. Other modules that import winmm (GOG's ori_d3d8.dll,
// dshow.dll, dinput8.dll) bind to this module too and get the same forwards.
// The mod does nothing but forward unless the host is "SILENT HILL 4.exe" (SH4Config.exe, dxcfg.exe and other tools
// in the folder may load it as well). It shares no state with the Playable Eileen dsound.dll: either can be installed
// alone, and their DllMain order does not matter. Files next to it: SH4Refined.ini (settings, created on first start,
// importing [Window] from SH4PlayableEileen.ini), SH4Refined.log (+ .prev.log). v2.1.1 renamed them from SH4WindowMod.*
// (file_names.inc moves the old ones on the first start). v3.40 LG340: logs\ keeps the last 8 sessions' logs (logrotate.inc).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <share.h>
#include <math.h>
#include <float.h>  // v3.22.0: _finite (DimDraw)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#include "interp.h"  // v2.1: FRAME INTERPOLATION

#ifndef SH4WM_VERSION  // a test build may name itself (cl /DSH4WM_VERSION=...)
#define SH4WM_VERSION "3.49.0"  // v3.49 MERGE: 3.49.0 (GT349 Ghost Teleport held = continuous warps after each fade-in, no cooldown, never across or into a walk-in event zone, no warp at a door / room change / fade / script; SH349 Shadow Darkness on the Tremers' wall shadows ([Shadows] TremerShadows, hook 0x45F4CA); SB349 the save / load list's highlight bar in the game's blue ([SaveBanks] ListBar, 11 bytes at 0x56FDCF) + the pointer kept on the Bank row; LK349 [Input] OmniLook: Room 302's analog right-stick look; EF349 Enemies Follow Victims Only (values 0 Off / 1 Monsters / 2 Victims Only / 3 Monsters + Victims); RZ349 Randomizer Monsters Off / This Room / This World + the save filter 0x465468; RZ349L Randomizer Eileen Outfit (companion) and Ghosts Spots; the MSG_RZ1 text block); v3.48 MERGE: 3.48.0 (OM348 Option 2's row is Omni-Directional Movement Off / On, On = v3.47's Analog, Direction removed and migrated to On; SB348 the save list's Bank row: Left / Right 1 bank, Q / E and the shoulders 10 banks (round 2: not the triggers), the mouse held on an arrow repeats, one log line per settled change; RZ348 the Randomizer folder on Option 5: Seed, New Seed, Items Off / Shuffle / Random, Keep Specials, Room 302; round 2: weapons in the pool, the pickup looks like its new item, Change Each Visit); v3.47 MERGE: 3.47.0 (PX347 the pixel checks read each cell at its centre + the source, DF347 the depth of field eases in, SB347 the save list's Bank row in the Menu Font + clickable arrows, CT347 Stick Movement on Option 2 (Control) + SH4R_StickAmount export 199 + the focus-loss line, RG347 Refined's own Regeneration); v3.46 OM346 (Stick Movement: 360-degree / analog left stick); v3.45 MERGE (QS345 the player texts + DoF Low / Medium / High + items log once, RB345 a read back that works under dgVoodoo2); v3.44 MERGE (CS344 shader forms + pixel check + Bloom Very High, DF344 DoF alignment + Screen Effects rows + lighter panels, GT344 Run+Action / Richard fade / forward warp, FN344 Menu Font row + 16-bit renderer removed); v3.43 MERGE: 3.43.0 (SH343 shadow c1 read-back + Darkness 40 / 25 %, GT343 Ghost Teleport, QC343 Quick Charge (replaces Tireless; export 198), DF343 Depth of Field, PP343 Bloom / Colour Grading / Otherworld Lens, FN343 HD menu font); v3.42 MERGE (EG342 2D pixel alignment + edge quads, SM342 Room 302 DP off / Curse Meter Off-On / shadow Darkness / peep gate); v3.41 MERGE: 3.41.0 (SH341 one Soft + pause picture on the GPU, EN341 ghost copies, VW341 window views / edges, DP341 Death Presence gate); v3.40 MERGE: 3.40.0 (SH340 shadows + LG340 logs); v3.39 PR339: 3.39.0; v3.39 PR339 (Presentation clean-up, Renderer, Opening, F7 preview); v3.38.1 HF3381: 3.38.1; v3.38.1 HF3381 (the curse meter fades under Hide Gauge: no wake on a drift); v3.38 MERGE: 3.38.0 (HB338 + EN338 + DR338; v3.37 MERGE: 3.37.0, MN337 + INV337 + CP337; v3.36.1 TL3361: 3.36.1; v3.36 MERGE: 3.36.0; v3.35.1 HF3351: 3.35.1; v3.35 W0: 3.35.0; v3.34 PEC334: 3.34.0; v3.33 LOG333: 3.33.0; v3.32 MERGE: 3.32.0; v3.31 MERGE: 3.31.0; v3.30 MERGE: 3.30.0; v3.29 merge: was still "3.27.2" in v3.28.0)
#endif

static HMODULE g_self;
static FILE *g_log;
static char g_dir[MAX_PATH], g_iniPath[MAX_PATH];

// v3.33 LOG333: one locked writer for every level (lines from the game thread and the render thread never interleave)
static CRITICAL_SECTION g_logCs;
static LONG g_logCsInit;  // 0 none, 1 initialising, 2 ready
static volatile LONG g_logBytes;  // v3.33 LOG333 (W7): bytes written this session (LogV / LogT stop at LOG_VERBOSE_CAP)
#define LOG_VERBOSE_CAP (32L << 20)
static void LogLine(const char *prefix, const char *fmt, va_list ap)
{
    SYSTEMTIME t;
    if (!g_log) return;
    if (g_logCsInit != 2) {
        if (InterlockedCompareExchange(&g_logCsInit, 1, 0) == 0) {
            InitializeCriticalSection(&g_logCs);
            InterlockedExchange(&g_logCsInit, 2);
        } else {
            while (g_logCsInit != 2) Sleep(0);
        }
    }
    EnterCriticalSection(&g_logCs);
    GetLocalTime(&t);
    {   // v3.33 LOG333 (W7): count the bytes (the verbose cap)
        int a = fprintf(g_log, "%02d:%02d:%02d.%03d %s", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, prefix);
        int b = vfprintf(g_log, fmt, ap);
        g_logBytes += (a > 0 ? a : 0) + (b > 0 ? b : 0) + 1;
    }
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_logCs);
}
static void Log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    LogLine("", fmt, ap);
    va_end(ap);
}
// v3.33 LOG333: log levels ([Log] Level, hidden: 1 normal = v3.32, 2 verbose = the default during QA rounds, 3 trace).
// LogV: decisions a tester needs (why a feature did or did not act), rate-limited by the caller (LogQuota).
// LogT: per-tick detail, only when asked for. LogAnomaly: always written, "ANOMALY: <area>: ..." so one grep finds every
// state a feature should never reach (a hook that never fires while its row is on, a value out of range, a fault).
static int g_logLevel = 2;
static volatile LONG g_logAnomalies;  // this session's ANOMALY lines (the first 300 are written, then one note)
static int LogVerboseFull(void)  // v3.33 LOG333 (W7): past the cap, verbose / trace lines stop (one note)
{
    static volatile LONG s_said;
    if (g_logBytes < LOG_VERBOSE_CAP) return 0;
    if (!InterlockedExchange(&s_said, 1)) Log("log: %ld MB written this session: verbose and trace lines stop here "
                                              "(normal lines and ANOMALY lines go on)", (long)(LOG_VERBOSE_CAP >> 20));
    return 1;
}
static void LogV(const char *fmt, ...)
{
    va_list ap;
    if (g_logLevel < 2 || LogVerboseFull()) return;  // v3.33 LOG333 (W7): + the cap
    va_start(ap, fmt);
    LogLine("[v] ", fmt, ap);
    va_end(ap);
}
static void LogT(const char *fmt, ...)
{
    va_list ap;
    if (g_logLevel < 3 || LogVerboseFull()) return;  // v3.33 LOG333 (W7): + the cap
    va_start(ap, fmt);
    LogLine("[t] ", fmt, ap);
    va_end(ap);
}
static void LogAnomaly(const char *area, const char *fmt, ...)
{
    va_list ap;
    char pre[80];
    const LONG n = InterlockedIncrement(&g_logAnomalies);
    if (n > 300) {
        if (n == 301) Log("ANOMALY: log: more than 300 anomaly lines this session; the rest are counted only");
        return;
    }
    sprintf_s(pre, sizeof pre, "ANOMALY: %s: ", area ? area : "?");
    va_start(ap, fmt);
    LogLine(pre, fmt, ap);
    va_end(ap);
}
// A per-site quota: 1 while *n (a static counter of the caller) is under max (verbose lines that could repeat each tick).
static int LogQuota(volatile LONG *n, LONG max) { return InterlockedIncrement(n) <= max; }
#include "pools_sim.h"  // v3.36 TP336: the task pool's size g_tpN and range helper TaskSlotOk / TaskSlotAt (every walker)

// ================================================================ winmm forwarding

static FARPROC g_pxFn[1024];  // resolved real functions, by export index (PX_COUNT used)
static HMODULE g_real;

static const struct { const char *name; WORD ord; const char *alias; } kPx[] = {
#define PX_TABLE
#include "winmm_exports.inc"
#undef PX_TABLE
};

// The real 32-bit winmm.dll, by full path: SysWOW64 on 64-bit Windows, System32 on 32-bit Windows.
static HMODULE RealWinmm(void)
{
    HMODULE m = g_real, prev;
    WCHAR p[MAX_PATH + 16];
    UINT n;
    if (m) return m;
    n = GetSystemWow64DirectoryW(p, MAX_PATH);
    if (!n || n >= MAX_PATH) n = GetSystemDirectoryW(p, MAX_PATH);  // 32-bit Windows: System32 holds the 32-bit dll
    if (!n || n >= MAX_PATH) return NULL;
    wcscpy_s(p + n, MAX_PATH + 16 - n, L"\\winmm.dll");
    m = LoadLibraryExW(p, NULL, 0);
    if (m == g_self) {  // never forward to ourselves
        FreeLibrary(m);
        return NULL;
    }
    if (!m) return NULL;
    prev = (HMODULE)InterlockedCompareExchangePointer((PVOID volatile *)&g_real, m, NULL);
    if (prev) {
        FreeLibrary(m);
        m = prev;
    }
    return m;
}

// Called by a stub on its first use (cdecl, from naked code that keeps ecx/edx).
FARPROC __cdecl PxResolve(int i)
{
    DWORD err = GetLastError();
    HMODULE m = RealWinmm();
    FARPROC f = NULL;
    if (m) {
        if (kPx[i].name) {
            f = GetProcAddress(m, kPx[i].name);
        } else {
            f = GetProcAddress(m, MAKEINTRESOURCEA(kPx[i].ord));
            if (!f && kPx[i].alias) f = GetProcAddress(m, kPx[i].alias);
        }
    }
    if (f)
        InterlockedExchangePointer((PVOID volatile *)&g_pxFn[i], (PVOID)f);
    else
        Log("winmm: cannot resolve %s (ordinal %u) from the system winmm.dll (%p); the call fails", kPx[i].name ? kPx[i].name : "(no name)",
            kPx[i].ord, m);
    SetLastError(err);
    return f;
}

// One stub per export: jump into the real function (stack untouched, so any calling convention passes through).
// Failure (no real dll): return MMSYSERR_ERROR with the stdcall argument bytes popped.
#define PX_STUB(i, argBytes)                                                                        \
    __declspec(naked) void PX_##i(void)                                                             \
    {                                                                                               \
        __asm mov eax, dword ptr [g_pxFn + (i) * 4]                                                 \
        __asm test eax, eax                                                                         \
        __asm jz slow                                                                               \
        __asm jmp eax                                                                               \
        __asm slow:                                                                                 \
        __asm push ecx                                                                              \
        __asm push edx                                                                              \
        __asm push i                                                                                \
        __asm call PxResolve                                                                        \
        __asm add esp, 4                                                                            \
        __asm pop edx                                                                               \
        __asm pop ecx                                                                               \
        __asm test eax, eax                                                                         \
        __asm jz fail                                                                               \
        __asm jmp eax                                                                               \
        __asm fail:                                                                                 \
        __asm mov eax, 1                                                                            \
        __asm ret argBytes                                                                          \
    }
#include "winmm_exports.inc"

// ================================================================ patch list export

// For other tools (the cheat table's restoreGameCode, which rewrites .text from the exe on disk): every range of the
// game's image this mod has changed. Same layout as the Playable Eileen plugin's SH4PE_Patches: magic 'SHWM', version,
// count, then count pairs of {VA, length}. Filled at startup (DllMain) and by the watcher thread (a late DirectInput
// import hook); entries are only added.
typedef struct { DWORD magic, version, count; struct { DWORD va, len; } item[512]; } PatchListExport;  // v2.0: 512 (was 64)
PatchListExport SH4WM_Patches = { 0x4D574853, 1, 0 };  // exported by winmm.def (DATA)

static void ListPatch(DWORD va, DWORD len)
{
    static volatile LONG s_lock;  // v2.0 (review): the window thread and the game thread may both add
    DWORD n;
    while (InterlockedExchange(&s_lock, 1)) Sleep(0);
    n = SH4WM_Patches.count;
    if (n < sizeof SH4WM_Patches.item / sizeof SH4WM_Patches.item[0]) {
        SH4WM_Patches.item[n].va = va;
        SH4WM_Patches.item[n].len = len;
        MemoryBarrier();
        SH4WM_Patches.count = n + 1;
    }
    InterlockedExchange(&s_lock, 0);
}

// ================================================================ settings (SH4Refined.ini)

#include "file_names.inc"
#include "logrotate_sim.h"  // v3.40 LG340: the last [Log] Keep sessions' logs in logs\ (names, prune plan; test\logrotate_test.c)
#include "logrotate.inc"  // v3.40 LG340: the copies and deletes (Init, before RenameOldFiles)

// Same layout as SH4PlayableEileen.ini. (v3.48 MERGE: SH4PlayableEileen/staging/winsplit/split_settings.py is the frozen v0.82 split's reference for test/run_tests.py, [Window] + ControllerCrashFix only; it no longer follows these tables and needs no edit per key: merge348_tools/split_settings.patch)
typedef struct { const char *section, *key, *def, *text; } KeyInfo;
static const char *const kIniHeader[] = {
    "SH4Refined settings for SH4 Refined (formerly SH4 Window Mod; winmm.dll) for Silent Hill 4.",
    "Lines that start with a semicolon are comments. The game ignores them.",
    "The game reads these values when it starts. Change them while the game is closed.",
    "Switches use 1 for on and 0 for off.",
};
static const struct { const char *name, *title; const char *about[3]; } kIniSections[] = {
    { "Window", "Window", { "Runs the game in a window that you can move and resize, keeps the game",
                            "running when you switch to another window and keeps the mouse inside it.",
                            "These values need a game restart." } },
    { "Input", "Input", { "Controller fixes and Omni-Directional Movement. OmniMove / OmniDeadZone / OmniLook apply at once (also when",
                          "edited here while the game runs); the others need a game restart.", NULL } },  // v3.46 OM346 (was: Controller fixes. These values need a game restart.)  // v3.48 OM348: the name  // v3.49 LK349: + OmniLook
    { "Saves", "Saves", { "Save titles. These values need a game restart.", NULL, NULL } },  // v3.5.2
    { "ExtraMenu", "ExtraMenu", { "The pause menu's Extra Menu (Options > Option 6 Extra Menu sets the rows). Enabled needs a",
                                  "game restart; the rows apply at once.", NULL } },  // v3.7.0
    { "EileenOutfit", "EileenOutfit", { "Eileen's outfit as a companion (the pause menu's Extra Menu row Eileen). Enabled needs a",
                                        "restart; the others apply at once.", NULL } },  // v3.9.0
    { "Camera", "Camera", { "The over-the-shoulder camera (Options > Option 6 > Camera: OTS / Game; the pause menu's Extra Menu >",
                            "Camera: the settings; the camera keys or the right stick turn it). These apply at once. While Playable",
                            "Eileen still has its own camera, its [Camera] in SH4PlayableEileen.ini is used instead (and copied here)." } },  // v3.33 CAM333; v3.37 CP337: the settings in the pause menu
    { "SaveBanks", "SaveBanks", { "Save Banks: more save files (Options > Option 6, or Q / E in the save and load lists). Bank 1",
                                  "is the game's own save\\sh4.sav, bank N is save\\sh4_bankN.sav. Enabled and Bank apply at once,",
                                  "ListBar at the next start." } },  // v3.8.0  // v3.49 SB349: ListBar (was: Both apply at once.)
    { "QuickSave", "QuickSave", { "Quick Save / Quick Load in game (3 rotating slots, kept after a restart). These",
                                  "values need a game restart.", NULL } },  // v3.6.0
    { "Textures", "Textures", { "Texture replacement (prototype, off by default). A replacement is <key>.dds in Folder,",
                                "the key from the texture extractor's manifest. These values need a game restart.", NULL } },
    // v3.3.5: [Items] had no entry here, so a new file got its keys appended by IniInt, without comments
    { "Items", "Items", { "Item Shine: a shine on items and memos lying in the worlds (Options > Option 5 Gameplay > Item Shine).",
                          "These values are read again while the game runs.", NULL } },
    { "Graphics", "Graphics", { "V-Sync, anti-aliasing and anisotropic filtering are set in GOG's DirectX Settings (GOG Galaxy >",
                                "Manage > DirectX Settings, dxcfg.exe). Hidden keys of this section: see README.", NULL } },  // v3.12.0; v3.39 PR339: the rows are gone
    { "Subtitles", "Subtitles", { "Subtitle style (Options > Option 3 > Subtitles, press Enter). These apply at once.", NULL, NULL } },  // v3.13.0
    { "Gameplay", "Gameplay", { "Cheats (Options > Option 5 Gameplay > Player / Enemies / Special). They apply at once from the menu;",
                                "these values are read again while the game runs. Normal / Off = the game's own.", NULL } },  // v3.25.0
    { "Randomizer", "Randomizer", { "The Randomizer (Options > Option 5 Gameplay > Randomizer): item pickups, monsters, Eileen's outfit and ghost spots",  // v3.49 RZ349: + monsters  // v3.49 MERGE: + RZ349L's outfit and ghost spots
                                    "from one seed. Changes apply at the next room load; nothing is written into saves. Read again while the game runs.", NULL } },  // v3.48 RZ348
    { "ScreenEffects", "ScreenEffects", { "Screen Effects (Options > Option 3 > Noise Effect, press Enter): film grain strength and look,",
                                          "and a filter over the scene in gameplay (HUD and text stay clean). These apply at once; the",
                                          "Thermal type lists (they also pick the ghosts Spirit Vision shows) are read at the start." } },  // v3.15.0; v3.16.0
    { "Shadows", "Shadows", { "Real-time shadows (Options > Option 3 > Realtime Shadow, press Enter). These apply at once.", NULL, NULL } },  // v3.40 SH340
};
static const KeyInfo kIniKeys[] = {
    { "Window", "Enable", "1", "Windowed mode. 1 means on, 0 means off." },
    { "Window", "Width", "0", "Window width in pixels. 0 chooses automatically (3/4 of the screen height, in shape)." },
    { "Window", "Height", "0", "Window height in pixels. 0 chooses automatically (3/4 of the screen height)." },
    { "Window", "Resizable", "1", "You can drag the window edges to resize it. 1 means on, 0 means off." },
    { "Window", "KeepActiveOnAltTab", "1", "Game keeps running when you switch window. 1 means on, 0 means off." },
    { "Window", "CaptureMouse", "1", "Cursor stays inside the window. 1 means on, 0 means off." },
    { "Window", "SnapToStandard", "1", "Resizing snaps to standard sizes. 1 means on, 0 means off." },
    { "Window", "NoMinimize", "1", "Game never minimises itself, also in full screen. 1 means on, 0 means off." },
    { "Window", "AspectMenu", "1", "Aspect Ratio (4:3, 16:10, 16:9, 21:9, 32:9) in Options, 3D view right at every size. 1 means on, 0 means off." },
    { "Window", "Menus43", "1", "Menus, text, HUD and movies stay 4:3 in the middle in wide shapes. 1 means on, 0 means off." },
    { "Window", "MenuPillars", "1", "Black sides beside full-screen pictures (memos, Scrap Book, map, save / load screens) in wide shapes. 1 means on, 0 means off." },
    { "Window", "Menus43Log", "0", "Diagnostic: the log names each new full-width 2D draw once. 1 means on, 0 means off." },
    // v3.39 PR339: [Graphics] VSync / AntiAliasing / Anisotropic (v3.12.0) and PerfOverlay (v3.37, now hidden) left this table
    { "Subtitles", "Size", "Normal", "Subtitle size: Small, Normal (the game's), Large or Huge. Long lines are fitted to the screen." },
    { "Subtitles", "Colour", "White", "Subtitle colour: White (the game's), Yellow, Cyan, Green, Red, Pink, Orange, Amber or Ice Blue." },
    { "Subtitles", "Edge", "Shadow", "Around the letters: Shadow (the game's), None, Outline or Outline+Shadow." },
    { "Subtitles", "Box", "None", "Behind the subtitle: None (the game's), Translucent or Solid (a dark box)." },
    { "Subtitles", "Effect", "None", "How a line appears and moves: None, Typewriter (with a cursor), Fade In (in and out as a whole), Wave, Glitch (blocks with colour fringes), Blood Drip or Embers." },
    { "Subtitles", "Exit", "None", "How a line leaves: None, Fade Out, Burn Out or Paper Peel." },
    { "ScreenEffects", "NoiseStrength", "Normal", "Film grain and dust when Option 3 > Noise Effect is On: Low, Normal (the game's), High or Very High." },
    { "ScreenEffects", "NoiseType", "Stock", "The grain's look: Stock (the game's), Fine, Coarse, Film (grain and dust), VHS (line noise), Scan Roll (a rolling band) or Rain (drops on the lens)." },
    { "ScreenEffects", "PreviewImage", "preview.dds", "A picture behind the Screen Effects page (DDS: DXT1/3/5 or 32-bit; your own, nothing is shipped). Relative to the game folder." },
    { "ScreenEffects", "PreviewCapture", "1", "1: the first pause in gameplay saves that (unfiltered) picture as PreviewImage when the file does not exist. 0: off." },
    { "ScreenEffects", "NoiseColour", "Stock", "The grain's colour: Stock (each type's own), Mono, Full Colour, Red, Green, Blue, Amber or Sepia." },
    { "ScreenEffects", "Filter", "None", "Filter over the scene in gameplay: None (the game's), CRT, Vivid, Sepia, Old Film, Night Vision, Night Vision Mono, Thermal, Spirit Vision (a black world of edge lines; ghosts, their damage areas, wall portals, holy candles and soul flames glow through walls) or Vignette." },
    { "ScreenEffects", "ThermalGhost", "", "Thermal: more enemy types to hide, hex (08 = Twin Victim). The ghosts 0B to 0F are hidden already." },
    { "ScreenEffects", "ThermalNotGhost", "", "Thermal: ghost types to show (hot) instead: 0B victims, 0C Cynthia, 0D Jasper, 0E Andrew, 0F Richard." },
    { "ScreenEffects", "ThermalCoolSeconds", "30", "Thermal: seconds a dead enemy takes to cool from hot to cold (0 = at once, up to 600)." },
    { "ScreenEffects", "SoulFadeSeconds", "6", "Spirit Vision: seconds a dead enemy's soul fire takes to die down (0 = at once, up to 600)." },
    { "ScreenEffects", "ThermalCold", "", "Thermal: enemy types drawn without the heat boost, hex (03 = Hummer)." },
    { "ScreenEffects", "Cutscenes", "On", "On: the filter also runs in real-time cut-scenes. Off: only in play (movies are never filtered)." },
    { "ScreenEffects", "ThermalHeat", "1", "Thermal: 1 = bodies glow in heat colours over a cold world. 0 = the older brightness-based look." },
    { "ScreenEffects", "SpiritGhost", "Blood", "Spirit Vision ghosts: Blood = a pale core bleeding into crimson, a crimson aura and blood where the tendrils grow; White = the v3.23.0 white glow. Read at start." },
    { "ScreenEffects", "SoulNone", "06", "Spirit Vision: enemy types without a soul fire, hex (06 = Wheelchair; the ghosts 0B to 0F never have one)." },
    { "Shadows", "Edge", "Stock", "The edge of every real-time shadow (the player, companions, enemies, ghosts, Room 302's hauntings, event and cut-scene figures: all the game casts): Stock (the game's hard edge) or Soft (about 12 px at 720 lines, scaled with the resolution; v3.40's Softer reads as Soft). The inside stays as dark as the game's. Shadows painted into the rooms' textures are not changed." },  // v3.40 SH340; v3.41 SH341: one soft edge (was Soft 3 px / Softer 6 px)  // v3.49 SH349: every real-time shadow (was "character shadows")
    { "ScreenEffects", "DepthOfField", "Off", "Depth of field: Off, Cut-scenes or Always. What lies far behind the player (or well in front of the player) blurs, more the farther it is from the player's depth; in cut-scenes the scene's own focus when it has one. Options > Screen Effects: the Depth of Field and DoF Strength rows." },  // v3.43 DF343  // v3.45 QS345: "her" -> the player; the rows' place (v3.44)
    { "ScreenEffects", "DofStrength", "Medium", "Depth of field strength: Low (gentle: the blur starts farthest from the player, half size only), Medium (v3.43 / v3.44's Low: half and quarter size) or High (sooner, up to an eighth). Also 0, 1, 2. A Low written by v3.43 / v3.44 is read once as Medium (the hidden DofStrengthV=345 marks a v3.45 value). The blur eases in at every strength (no edge where it starts)." },  // v3.43 DF343  // v3.45 QS345: Low / Medium / High, default Medium  // v3.47 DF347: eases in
    { "Shadows", "Darkness", "85", "How dark shadows are, in percent of the game's own (Shadows page: 100, 85, 70, 55, 40 or 25; any 0 to 100 here), for Stock and Soft alike, on top of Playable Eileen's [Lighting] ShadowDarkness. Every real-time shadow, and the Tremers' wall shadows (TremerShadows). Tinted shadows keep their colour." },  // v3.42 SM342  // v3.49 SH349: every shadow (was "character shadows"), the row's six values
    { "Shadows", "TremerShadows", "1", "1: Shadow Darkness also applies to the Tremers' (the leeches') wall shadows, a ribbon the game draws outside the real-time shadows (their edge stays their texture's: Soft cannot blur it). 0: the game's own. Needs a game restart." },  // v3.49 SH349
    { "ScreenEffects", "Bloom", "Off", "Bright lights glow (lamps, windows, flames): Off, Low or High. Never on the HUD or menus; cut-scenes follow Cutscenes." },  // v3.43 PP343
    { "ScreenEffects", "Grading", "Off", "Colour grading: Off, PS2 (greener, slightly foggy), Otherworld (cold, drained), Apartment (warm) or Bleach (half the colour, harder contrast). Not with the Filters that set the colours themselves." },  // v3.43 PP343
    { "ScreenEffects", "Lens", "Off", "Otherworld Lens: On = a slight colour fringe and edge warp that fades in near ghosts and hauntings (Room 302's too). Off." },  // v3.43 PP343
    { "ScreenEffects", "PreviewKey", "F7", "A key that saves the current clean gameplay frame as PreviewImage, the picture behind the Screen Effects page (F1 to F12, or a Windows key code). None = off." },  // v3.39 PR339: F7 (was None); v3.39 REV339: only a missing key gets it (an existing None stays)
    { "Window", "FitWrapper", "1", "GOG's dxcfg.ini draws at the game's own size, in a window. 1 means on, 0 means off." },
    { "Window", "Borderless", "0", "Whole screen without a frame (Options > Display Mode, Alt+Enter). 1 means on, 0 means off." },
    { "Window", "Monitor", "0", "Screen for the game (Options > Monitor). 0 means the main screen, 2 means \\\\.\\DISPLAY2." },
    { "Window", "MonitorName", "", "Written by the mod: the chosen screen's own name, to find it again." },
    { "Window", "MonitorPath", "", "Written by the mod: the chosen screen's device path, to find it again." },
    { "Window", "FOV", "0", "Third-person view angle in degrees, all worlds (Options > Option 4 Presentation > 3rd Person FOV): 60 to 120 in steps of 5, 0 is the game's own." },
    { "Window", "FOVFirstPerson", "0", "First-person view angle in degrees, Room 302 (Options > Option 4 Presentation > 1st Person FOV): 100 to 120 in steps of 5, 0 is the game's own (always used looking out of the window or through a peephole)." },
    { "Window", "FrameRate", "30", "Gameplay frames per second, 30 (the game's own) or 60 (see FrameMode). Menus stay 30. 120 and 180 are experimental: they need the hidden key HighFrameRates=1 in this section." },  // v3.31 FPS331 (tagged by the v3.31 MERGE)
    { "Window", "Opening", "Skip Logos", "The start (Options > Option 4 > Opening): Full (the game's own: the KONAMI logos, the notice, the title intro), Skip Logos (straight to the title) or Fast (also no first-start movie and the short title intro). Read at the start." },  // v3.39 PR339
    { "Window", "FrameMode", "0", "0: true frame rate, the game itself runs at FrameRate (v3.0). 1: the game stays at 30 and the picture gets in-between frames." },
    { "Window", "InterpAutoLower", "1", "FrameMode 1: lower the picture rate when this PC cannot keep up (tried again later). 0 = never (diagnosis)." },
    { "Window", "SmoothCutscenes", "0", "FrameMode 1: in-between frames in real-time cutscenes too. 0 means cutscenes show 30 frames a second." },
    // v2.1 ENTITY TICK GATE (research\framerate\v3\entity_speed.md)
    { "Window", "EntityGate", "0", "FrameMode 0: 1 = monsters think and move 30 times a second (drawn in between). 0 = monsters run at the full frame rate (v3.0 true rate)." },
    { "Window", "CompanionGate", "0", "FrameMode 0: 1 = Cynthia and Eileen (as companions) think 30 times a second (drawn in between). 0 = they run at the full frame rate (v3.1)." },
    { "Window", "EntitySmooth", "1", "FrameMode 0: monsters and companions are drawn at in-between positions between their steps. 1 means on, 0 means off." },
    { "Input", "ControllerCrashFix", "1", "Stops a crash when a controller connects. 1 means on, 0 means off." },
    { "Input", "PadNames", "Auto", "Pad button names in Options > Button Config: Auto (from the connected pad), Xbox, PlayStation, Nintendo or Generic." },
    { "Input", "Vibration", "Low", "Vibration strength when Options > Option 2 > Vibration is on: Low (the game's own), Medium or High (stronger motors; GOG's controller wrapper)." },
    { "Input", "PadGlyphs", "1", "1 = Button Config shows pad buttons as glyphs (Kenney Input Prompts, CC0) for Xbox, PlayStation and Nintendo pads; 0 = names." },
    { "Input", "OmniMove", "Off", "Omni-Directional Movement (Options > Option 2 (Control), above Exit): Off (the game's 8 directions) or On (the left stick walks at any angle, and a lighter push walks slower, down to the slowest walk; Playable Eileen v1.24.0 and later follows the push too). Also 0, 1. Direction, Analog and 2 from v3.46 / v3.47 read as On (written back once as On). Only in gameplay (menus, pauses and cut-scenes keep the game's input); the keyboard stays 8-way; the stick wins while it is pushed. Needs movement on the left stick (or the sticks swapped). Applies at once." },  // v3.46 OM346  // v3.47 CT347: the row on Option 2; Playable Eileen may follow the push  // v3.48 OM348: Omni-Directional Movement, Off / On (On = v3.47's Analog; Direction removed)
    { "Input", "OmniDeadZone", "0.30", "Omni-Directional Movement's dead zone (no Options row): how far the stick must be pushed, 0.05 to 0.90 of a full push, round (0.30 = the game's own); inside it the game's own input stays." },  // v3.46 OM346  // v3.48 OM348: the name
    { "Input", "OmniLook", "Room302", "The right stick's look while Omni-Directional Movement is On (no Options row): Room302 (Room 302's first-person view turns with the stick's push, any angle), All (every stage, also the third-person and over-the-shoulder look) or Off (the game's own 8-way look). Also 0, 1, 2. Needs the look on the right stick (or the sticks swapped); a look key held keeps the game's look. Applies at once." },  // v3.49 LK349
    { "ExtraMenu", "Enabled", "1", "An Extra Menu in the pause menu (needs a restart). 0 = the game's own pause menu." },
    { "ExtraMenu", "Chest", "1", "Extra Menu row Storage Chest: the item box anywhere. 1 shows the row, 0 hides it." },
    { "ExtraMenu", "Save", "1", "Extra Menu row Save Game: the save menu anywhere. 1 shows the row, 0 hides it." },
    { "ExtraMenu", "Load", "1", "Extra Menu row Load Game: the load list anywhere. 1 shows the row, 0 hides it." },
    { "ExtraMenu", "QuickSave", "1", "Extra Menu row Quick Save (same files as the Quick Save key). 1 shows the row, 0 hides it." },
    { "ExtraMenu", "QuickLoad", "1", "Extra Menu row Quick Load (the newest quick save). 1 shows the row, 0 hides it." },
    { "ExtraMenu", "Eileen", "1", "Extra Menu row Eileen (her outfit; from [EileenOutfit], or from the cheat table's Eileen Outfit when that is off). 1 shows the row, 0 hides it." },
    { "EileenOutfit", "Enabled", "1", "Eileen's outfit engine. 0 = the game's own outfit (no Eileen row unless the cheat table provides it)." },
    { "EileenOutfit", "Outfit", "-1", "-1 Default (the game decides), 0 Dress (her normal clothes), 1 Nurse. The pause menu row changes it." },
    { "EileenOutfit", "Instant", "1", "1 = a new outfit is put on at once while she is with you; 0 = at her next spawn (door, room load)." },
    { "EileenOutfit", "Cutscenes", "1", "Cutscene, peephole and door-viewer files: 0 = the game's outfit, 1 = follow the choice, 2 = follow, but the peephole keeps its own file. Her in-game model inside cutscenes always follows the choice." },
    // v3.33 CAM333: [Camera] (camera.inc), Playable Eileen's keys and meanings; Enable 0 = v3.32 (its first start imports PE's)
    { "Camera", "Enable", "0", "Over-the-shoulder camera in the worlds (Options > Option 6 > Camera; the settings: the pause menu's Extra Menu > Camera). 1 means on, 0 means the game's own cameras. Room 302 keeps its own view." },  // v3.37 CP337
    { "Camera", "Distance", "800", "How far behind the player the camera sits, 0 to 2000 (the menu: 0 to 1000). 0 is a first person view." },
    { "Camera", "Side", "0", "Shoulder offset, -500 to 500. Plus moves the camera right, minus left." },
    { "Camera", "Height", "330", "Camera height above the player's feet, 0 to 600 (the same for Henry and Playable Eileen's Eileen; her eyes are at about 300)." },
    { "Camera", "TiltDown", "17", "How far down it looks at the start, in degrees (-68 to 68). Minus looks up." },
    { "Camera", "Smoothing", "0.2", "How fast the view follows a turn, 0.01 to 1. 1 is at once." },
    { "Camera", "TurnSpeed", "0.1", "How fast the camera keys or right stick turn the view (0 to 1)." },
    { "Camera", "TiltSpeed", "0.1", "How fast the camera keys or right stick tilt the view (0 to 1)." },
    { "Camera", "InvertX", "0", "Turns the view the other way. 1 means on, 0 means off." },
    { "Camera", "InvertY", "0", "Tilts the view the other way. 1 means on, 0 means off." },
    { "Camera", "Follow", "0", "The view turns with the player. 0 means only the camera keys turn it." },
    { "Camera", "WallZoom", "1", "Moves in instead of going through walls. 1 means on, 0 means off." },
    { "Camera", "WallMargin", "50", "How far the camera keeps from walls, 0 to 200. Needs WallZoom on." },
    { "Camera", "ZoomOut", "0.06", "How fast it moves back out after a wall, 0 to 1. 1 is at once." },
    { "Camera", "ZoomIn", "0.25", "How fast it moves in near a wall, 0 to 1. 1 is at once." },
    { "Camera", "CeilingLimit", "450", "Highest camera point above the player's feet near walls. 0 is no limit." },
    { "Camera", "StairSmoothing", "0.25", "Smooths the height on stairs. 0 is off, 0.15 heavy, 0.4 light." },
    { "Camera", "Fov", "63", "Field of view in degrees (20 to 120) while this camera shows: rooms and running no longer zoom it. 0 is the game's. Option 4's 3rd Person FOV still scales it." },
    { "Camera", "FixedCameras", "0", "Room camera zones that take the view (fixed angles). 0 keeps this camera, 1 is the game's." },
    { "SaveBanks", "Enabled", "1", "Save Banks. 1 means on, 0 means the game's own single save file only." },
    { "SaveBanks", "Bank", "1", "The bank the save and load lists open on, 1 to 99 (Q / E in the lists change it too)." },
    { "SaveBanks", "ListBar", "Blue", "The highlight bar of the save and load lists: Blue (the game's blue menu bar, as on the Bank row) or Yellow (the game's own list bar). Also 1, 0. Read at the start." },  // v3.49 SB349
    { "QuickSave", "Enabled", "1", "Quick Save / Quick Load keys in game. 1 means on, 0 means off." },
    { "QuickSave", "SaveKey", "F5", "Key for Quick Save: F1 to F12, or a Windows key code. Not with Ctrl, Alt or Shift held." },
    { "QuickSave", "LoadKey", "F9", "Key for Quick Load (loads the newest of the 3 quick saves): F1 to F12, or a Windows key code." },
    { "QuickSave", "Folder", "save\\quick","Folder of the quick saves qs1.bin to qs3.bin, in the game folder (or a full path)." },
    { "QuickSave", "Feedback", "1", "A short line on screen after a quick save / load. 1 means on, 0 means sounds only." },
    { "Saves", "WorldTitles", "1", "A save made in a world is titled after that world (and its 2nd visit), not after the last return to Room 302. 1 means on, 0 means the game's own titles." },
    // items.inc (v3.3); v3.3.5: the Sparkle* keys of v3.3.0-v3.3.4 are renamed Shine* (ShineMigrate converts them)
    { "Gameplay", "MoveSpeed", "Normal", "Walk and run speed (the player's animation follows): Normal, x1.25, x1.5 or x2 (x2 may pass through thin walls)." },  // v3.25.0  // v3.45 QS345: "her" -> the player's
    { "Gameplay", "MedallionDurability", "Normal", "How long the Saint Medallion lasts against ghosts: Normal, x2, x3 or x4." },
    { "Gameplay", "OneHitKill", "Off", "One-Hit Kill (Option 5 > Player, the top row): your hits kill monsters at once (ghosts are knocked down; Walter and bosses as normal). Off: the game's own. Off or On." },  // v3.37 MN337: Enemies -> Player
    // PLAYER PACK (playercheats.inc): Option 5 > Player
    { "Gameplay", "AmmoSaver", "Normal", "Pistol and revolver rounds (Option 5 > Player): Normal, Saver (every other shot is free) or Endless (the clip never empties). Playable Eileen's SMG is not covered." },
    { "Gameplay", "DarkPact", "Off", "A trade: Pact = your hits on monsters, ghosts and bosses x1.5 and the damage you take x1.5; Blood = x2 both ways. Off, Pact or Blood. (Playable Eileen's SMG hits are not scaled.)" },
    { "Gameplay", "GrabBreaker", "Stock", "Ghost and Sniffer Dog grabs: Stock, Quick (half the hold) or Instant (you break free at once; the grab's first damage stays)." },
    { "Gameplay", "QuickCharge", "Off", "Quick Charge (Option 5 > Player): hold attack with a melee weapon and its charged swing is full 1.5, 2 or 3 times as fast (Henry's weapons, and Playable Eileen's when it is loaded). Off, 1.5x, 2x or 3x. Replaces v3.42's Tireless: an old Tireless line is removed at the start." },  // v3.43 QC343: was Tireless  // v3.43 MERGE: removed (was ignored)
    { "Gameplay", "LifeSteal", "Off", "Your hits on monsters heal you by 5 or 10 % of the damage (not ghosts, Walter or the Wall boss; at most 10 % of your max health a hit). Off, 5 or 10." },
    { "Gameplay", "Regen", "Off", "Regeneration (Option 5 > Player): after RegenDelay seconds without damage you heal slowly, up to full health: x1 = 15 %, x1.25 = 18.75 %, x2 = 30 % of your max health a minute (Playable Eileen's own rates); never while paused, in events, doors or cut-scenes, at 0 health, near a ghost or haunting aura, or while Room 302's own healing runs. For Henry, and for Playable Eileen while her own regeneration is off; while hers is on (SH4PlayableEileen.ini [Defense] Regen, its menu) the row is grey and shows Off. With a Playable Eileen before v1.23.2 the row sets its [Defense] Regen / RegenRate as in v3.46. The first start of v3.47 copies the value from SH4PlayableEileen.ini. Off, x1, x1.25 or x2." },  // v3.47 RG347
    { "Gameplay", "RegenDelay", "30", "Regeneration's wait (no Options row): seconds without healing after any damage, 10 to 120 (Playable Eileen's RegenDelay; the first start of v3.47 copies it from SH4PlayableEileen.ini)." },  // v3.47 RG347
    { "Gameplay", "SupplyDrops", "Off", "Supply Drops (Option 5 > Enemies): every kill of a monster and every full down of a ghost, a named ghost (Cynthia, Jasper, Andrew, Richard) or the stalker Walter rolls once for an item where it fell; on Normal about 1 roll in 8 (12 %) gives one. Common items (Nutrition Drink, Pistol / Revolver Bullets, a third each) come mostly from monsters, uncommon (Portable Medical Kit 2 : Holy Candle 1) and rare ones (Saint Medallion, Ampoule, Silver Bullets, Sword of Obedience, a quarter each) more from ghosts: rare 0.05 % a weak monster, 0.1 % a strong one, 0.25 % a ghost, 1 % Walter, 3 % a named ghost. Bullets only once you own that gun (Silver Bullets: the pistol). Every ghost down rolls the full chance. A new random seed at every game start, New Game, load and quick load (a reload rolls a kill anew; hidden key SupplyDropsSeed=Fixed: the same result every reload, as v3.29); drops lying are kept in the save. Off, Rare (half of Normal), Normal or Abundant (double; v3.37's Generous, which reads as Abundant)." },  // SUPPLY DROPS (v3.30 AS330: the session seed)  // v3.38 DR338: the four levels, the rarity groups (DR338b)  // v3.38 DR338c: the final design (one roll by class, no ghost falloff)
    { "Gameplay", "DeathPresence", "Off", "Death Presence (Option 5 > Special): a black-violet fog around you drains every being in it, more the longer it stays (Weak: 300 units, 2-8 % of max health a second; Strong: 400, 5-20 %). Eileen keeps back from it and takes a real haunting inside it; Cynthia recoils; downed ghosts stay down; enemies hesitate at its edge. Not in Room 302 (the hidden [ScreenEffects] DeathPresenceRoom302=1 brings back its slow clearing of the hauntings there). Off, Weak or Strong." },  // v3.27.0  // v3.42 SM342: off in Room 302
    { "Gameplay", "AfterImage", "Off", "After Shock (Option 5 > Special): while you move, a crackling trail of electric copies of you follows; any enemy it touches gets the Stun Gun's own hit (Gum Heads, Twin Victims and the Patient ghost go down for 3-10 s, dogs are floored, the rest flinch), then a cooldown of 3 s and 2 s on its feet (one stun per enemy whatever the length). Off in Room 302. Off, Short (3 copies over 0.2 s), Medium (5 over 0.35 s) or Long (8 over 0.6 s); v3.29's On reads as Medium." },  // v3.29 AFTERIMAGE (v3.30 AS330: After Shock, trail lengths)
    { "Gameplay", "Echo", "Off", "Echo Doubles (Option 5 > Special): shadow soldiers of you (near-black, each in a black flame coat tight to its figure with fine purple wisps, violet eyes; not attacking they stand farther from you at 45 % opacity, striking they turn solid) repeat your swings, stomps and shots from their own places; enemies react to their hits, but cannot hit or target them. Shadow Copy: they copy you, each late by its own random delay (0.15-0.6 s, changing every few seconds), turning toward their own targets; Shadow Cast: they copy you too, but while you attack each dissolves into smoke and reforms at an enemy on your floor, strikes it with your attack, and returns to you when you stop. While you set down a Holy Candle or pick an item up, they attack an enemy near you with your last attack, else stand idle beside you. Off, Shadow Copy or Shadow Cast (v3.29's Shadow, Twin and Mirror read as Shadow Copy). Hidden keys: EchoDamage (percent, 50: each shadow 75 / 60 / 50 % of your damage for 1 / 2 / 3), EchoSide (Auto / Right / Left), EchoColour (Violet / Blue / Crimson / White), EchoAura (1 = the flame, eyes and smoke; 0 = the bodies only), EchoRim (0; 1 = v3.30's edge shell), EchoDepth (2; 0 never / 1 every look / 2 Thermal and Spirit Vision), EchoMorphKeep (1)." },  // v3.30 ECHO; v3.30 ECHO330: Echo Doubles; v3.31 ECHO331: the flame, busy moments, keys; v3.32 ECHO332: the coat, calm vs striking
    { "Gameplay", "EchoCount", "1", "Echo Count (Option 5 > Special, under Echo Doubles): how many shadows, 1, 2 or 3, each in its own place beside or behind you (Shadow Cast: on other enemies, or round the same one)." },  // v3.30 ECHO330
    { "Gameplay", "NoClip", "Off", "No Clip (Option 5, the top row): On: you walk through walls, doors and props of any size; floors and stairs still carry you (you never fall: past the end of a floor you can walk on only a short way). Fly: you fly where the camera looks (look up and walk to rise, down to sink; you stay put without input), running at 2.5 x the run speed and walking at the run speed (hidden key NoClipFlySpeed=2.5 in this section: 1 to 4 times the run speed; Movement Speed does not change it); only while an over-the-shoulder camera is on, else it acts as On; leaving Fly sets you down on the floor below. Closed doors stay closed (they open only with the action button, with their locks); event walls keep their walls. Eileen and Cynthia catch up: one left behind a wall for 2 s after you went through one is placed beside you. Enemies keep their collision. Not in Room 302. Turned off while inside a wall it stays on until you step clear. Off, On or Fly (0, 1, 2)." },  // v3.32 NC332  // v3.33 NC333: + Fly  // v3.37 MN337: Option 5's top row, Fly's own speed
    { "Gameplay", "GhostTeleport", "Off", "Ghost Teleport (Option 5 > Special): hold Run while standing still (no movement input) for 0.4 s and you warp behind the nearest standing enemy within 2500 units, even through a wall, onto free floor behind its back in the same room, facing it, with ghost Richard's warp sound and Echo Doubles' teleport smoke; then a 3 s cooldown (one warp a press: let go of Run before the next). Not during events, cut-scenes, menus, hits, grabs or falls, not with your weapon raised, not in Room 302; no free floor behind the enemy = no warp. Henry and Playable Eileen alike. Off or On. Hidden keys: GhostTeleportRange (2500; 500 to 6000 units), GhostTeleportCooldown (3; 0.5 to 30 s), GhostTeleportSound (1; 0 = no warp sound)." },  // v3.43 GT343
    { "Gameplay", "EnemyCopies", "Off", "Enemy Copies (Option 5 > Enemies): every monster and ghost of a room you enter gets 1 to 3 copies near it (x2, x3, x4), the story ghosts too; never bosses, Walter, Wall Men, Tremers or mm. Ghost copies never die: a downed or pinned one is there again when you come back. The bone pool limits them (it grows to 4096 records at the next room change): crowded rooms get fewer. Killed copies do not count for the ending's kill rank and stay dead until you leave the world (or, with Revive Enemies, the room). Off, x2, x3 or x4." },  // v3.31 ENEMIES  // v3.30 EN330: the pool grows at a room change  // v3.31 EN331: ghosts
    { "Gameplay", "EnemyFollow", "Off", "Enemies Follow (Option 5 > Enemies): up to 3 monsters within 800 units of you come after you through a door, the first after 3 s; left behind in another room they stay there. Monsters + Victims: the victim ghosts too. Victims Only: only the victim ghosts follow; monsters stay in their rooms. Never through lifts, holes or Room 302. Off, Monsters, Victims Only or Monsters + Victims." },  // v3.31 ENEMIES  // v3.49 EF349: + Victims Only
    { "Gameplay", "EnemyRevive", "Off", "Revive Enemies (Option 5 > Enemies): leave a room and come back, and its killed monsters (and killed copies) stand again; their kills count for the ending's kill rank again. Never bosses, Walter, Wall Men, Wheelchairs or ghosts, never in story rooms. Off or On." },  // v3.30 EN330
    { "Randomizer", "Items", "Random", "Item pickups lying in the worlds: ammo, healing, Holy Candles, Saint Medallions, Swords of Obedience and (round 2) Henry's weapons that lie on their own; never keys, memos, Eileen's weapons or scripted pickups: Off, Shuffle (each world visit's own items trade spots: the same number of each) or Random (each spot draws an item as Supply Drops does, now and then a weapon). From the next room load." },  // v3.48 RZ348
    { "Randomizer", "KeepSpecials", "Off", "On: Holy Candles, Saint Medallions and Swords of Obedience stay where the game puts them (their spots and items leave the pool). Off: they are mixed in too." },  // v3.48 RZ348
    { "Randomizer", "Room302", "In", "In: Room 302's pickups are randomized too. Out: they stay as they are." },  // v3.48 RZ348
    { "Randomizer", "Monsters", "This World", "Monsters: Off, This Room (each monster of a room's script rolls its own type among the monster types whose files that room has loaded) or This World (also among every monster type this world places; their files are loaded at the room load). A mix per monster, any strength; story, boss and event monsters, ghosts, Walter, Wall Men, Wheelchairs and mm never change; Gum Heads only with Playable Eileen's crash guard. Seeded like the items (Seed, Change Each Visit). From the next room load; nothing randomized is written into saves." },  // v3.49 RZ349
    { "Randomizer", "EileenOutfit", "Random", "Eileen Outfit: Random = companion Eileen wears an outfit drawn among the ones this save has unlocked (the Dress; the Nurse once the game has unlocked it), a new draw at every world visit (Change Each Visit Off: the same one for this seed); a choice in the pause menu's Extra Menu row Eileen holds until the next world visit. Off = the outfit set in [EileenOutfit] Outfit / the Extra Menu. With Playable Eileen only the companion changes (the player's outfit is Playable Eileen's [Variant]). Needs [EileenOutfit] Enabled 1. Never written into saves or into [EileenOutfit] Outfit." },  // v3.49 RZ349L
    { "Randomizer", "Ghosts", "Spots", "Ghosts: Spots = the victim ghosts start at another spot of their own room (seeded; Change Each Visit On: a new spot at every visit), never the story ghosts (Cynthia, Jasper, Andrew, Richard); a ghost the game brings back from its record keeps its place. Off = where the game puts them. From the next room load." },  // v3.49 RZ349L
    { "Randomizer", "PerVisit", "On", "Change Each Visit: On = Random draws new items at every visit of a room, Shuffle at every visit of a world (a room keeps its items while you stay in that world); untaken spots only, nothing saved. Off = a room always has the same items for this seed." },  // v3.48 RZ348 r2
    { "Randomizer", "Seed", "0", "The seed every choice comes from (a whole number, 1 to 4294967295): the same seed gives the same items. 0 or missing: a new one is rolled at the next start and written here. Options > Randomizer > New Seed rolls another." },  // v3.48 RZ348
    { "Gameplay", "EnemyHealthBars", "Off", "A thin health bar over monsters in view (Option 5 > Enemies): Off, Locked Target (your lock-on target and monsters that lost health in the last 4 s) or All Near (also every monster within 1500 units). Ghosts show their down timer while down; bosses get a wider bar." },  // HUD BARS
    { "Gameplay", "ExtendedToolbelt", "Off", "Extended Toolbelt (Options > Option 5): a second toolbelt page, 20 items in all. Left / Right past the first or last item flips the page (also in the Storage Chest's belt row); a full first page sends one item to page 2 (a broken or spare weapon first, what you hold or wear last). Doors, events, the weapon in your hand, its ammo and the Saint Medallion work from either page. Page 2 is the chest's last 10 entries (the chest holds 126 while it is on) and is kept in the save. Off with items on page 2: the weapon in hand goes back to the first page, the rest into the Storage Chest (it stays on if the chest is too full). Turn the cheat table's Extend Toolbelt (7310) off when using this. Off or On." },  // v3.37 INV337
    { "Gameplay", "StackableItems", "Off", "Stackable Items (Options > Option 5): the same items stack on the toolbelt, up to 99 an entry: pickups, gifts and chest take-outs (the whole chest stack comes out as one entry) join a stack first; using one takes one; a stacked golf club, Wine Bottle or Saint Medallion that is used up costs one and the broken one is added. Counts above 1 are drawn. Off: the game's rules again, stacks stay as they are (the Gum Head steal cap and the stacked-break guard stay on). Turn the cheat table's Stackable Items (7360) off when using this. Off or On." },  // v3.37 INV337
    { "Gameplay", "CurseMeter", "Off", "Companion Eileen's curse meter, split into its stages Clear, Haunted, Cursed and Possessed (Option 5, under No Clip): Off or Companion (On in the menu). Her ring sits top right, or small beside Playable Eileen's own ring while that one shows; grey with LOCKED while Playable Eileen's [Ghost] LockCurse holds her curse at 0. Your own curse ring is Playable Eileen's (its pause menu): Refined no longer draws it. Older values: On and Player read as Off, Both as Companion." },  // v3.42 SM342: Off / On, companion only (v3.37 MN337: Player / greyed / Clear  // HUD BARS  // v3.32 MERGE: + Both (DHS332) and LOCKED)
    { "Items", "Shine", "1", "A shine on items and memos lying in the worlds (Options > Option 5 Gameplay > Item Shine). 1 means on, 0 means off." },
    { "Items", "ShineMemos", "1", "Memos and documents shine too. 1 means on, 0 means items only." },
    { "Items", "ShineRange", "3000", "How far away the shine shows, in game units (Eileen is about 340 tall). 500 to 20000." },
    { "Items", "ShineThroughWalls", "0", "1 shows the shine through walls too. 0 hides it behind walls." },
    { "Items", "ShineBodies", "1", "Eileen, enemies and other characters in front hide the shine. 1 means on, 0 means off." },
    { "Items", "ShineDepthTest", "0", "Experimental: hide the shine by the depth buffer (v3.3.2 hid every one). 0 means off." },
    { "Items", "ShineEvery", "7", "Seconds between an item's flashes, 2 to 60." },
    { "Items", "ShineFor", "2", "How long each flash lasts, in seconds (0.5 to the time between flashes)." },
    // texpack.inc (prototype, 2026-11-09)
    { "Textures", "Pack", "0", "Replace game textures with the <key>.dds files in Folder. 1 means on, 0 means off." },
    { "Textures", "Folder", "textures", "Folder of the replacement .dds files, in the game folder (or a full path)." },
    { "Textures", "LogTextures", "0", "Diagnostic: the log names the key of every texture the game loads, once. 1 means on, 0 means off." },
    // v3.4.0 (texpack.inc)
    { "Textures", "Pool", "0", "Where replacements are kept: 0 = MANAGED (a copy also in the game's own memory), 1 = DEFAULT (video memory only; refilled from the files after a device reset)." },
    { "Textures", "MenuFont", "Stock", "The font of the game's text (menus, memos, subtitles): Stock, or an HD look-alike drawn at 4x from fonts\\<name>.sh4font (OFL fonts: EBGaramond, CormorantGaramond, CrimsonText). The widths stay the game's, so no text moves. Options > Option 1 > Menu Font switches it at once." },  // v3.43 FN343 (font.inc)  // v3.44 FN344: + the row
};
#define KEY_COL 19
#define NKEYS ((int)(sizeof kIniKeys / sizeof kIniKeys[0]))

// v3.12.0 had Option 4 rows V-Sync / Anti-Aliasing / Anisotropic Filtering answering GOG's dxcfg.ini through the
// wrapper's GetPrivateProfileStringW import. v3.39 PR339 (USER_FEEDBACK_v3381 item 1): gone; GOG's DirectX Settings tool
// (dxcfg.exe) sets the same values in dxcfg.ini and the hook passes those keys through. An old [Graphics] VSync /
// AntiAliasing / Anisotropic that is not Default is named once in the log at start (ReadIni).
static const char *const kGfxIniKey[3] = { "VSync", "AntiAliasing", "Anisotropic" };  // v3.39 PR339: only for that note
static volatile LONG g_inDiscard;          // hkDiscard is inside 524320 (SetResIndex must not apply then)

// v3.13.0: subtitle style (subtitles.inc; staging/subtitles/PLAN.md), set on the Subtitle Style page, a hidden logical
// page (LP_SUB) opened by Enter on Option 3's stock Subtitles row.
enum { SUB_SIZE, SUB_COLOUR, SUB_EDGE, SUB_BOX, SUB_EFFECT, SUB_EXIT, SUB_N };  // v3.20.0: + Effect, Exit
#define SUB_VMAX 9
static const int kSubCount[SUB_N] = { 4, 9, 4, 3, 7, 4 };  // v3.21.0: Effect 7  // v3.19.0: Colour 9 (Grey -> Red, + Pink, Orange, Amber, Ice Blue)
static const char *const kSubIniKey[SUB_N] = { "Size", "Colour", "Edge", "Box", "Effect", "Exit" };
static const char *const kSubName[SUB_N][SUB_VMAX] = {
    { "Small", "Normal", "Large", "Huge" },
    { "White", "Yellow", "Cyan", "Green", "Red", "Pink", "Orange", "Amber", "Ice Blue" },
    { "Shadow", "None", "Outline", "Outline+Shadow" },
    { "None", "Translucent", "Solid" },
    { "None", "Typewriter", "Fade In", "Wave", "Glitch", "Blood Drip", "Embers" },  // v3.21.0: - Tremble, Flicker -> Blood Drip
    { "None", "Fade Out", "Burn Out", "Paper Peel" },
};
static const int kSubStock[SUB_N] = { 1, 0, 0, 0, 0, 0 };  // Normal, White, Shadow, None, None, None
static const float kSubScale[4] = { 0.85f, 1.0f, 1.15f, 1.3f };
static const DWORD kSubRgb[SUB_VMAX] = { 0x808080, 0x008080, 0x808000, 0x208020, 0x181880, 0x6C4080, 0x004880, 0x006480,
                                         0x807460 };  // 0xBBGGRR, 0x80 = full
static volatile LONG g_sub[SUB_N] = { 1, 0, 0, 0, 0, 0 };
static LONG g_snapSub[SUB_N];
static int g_subOn;            // the hooks are installed
#define LP_SUB 6                   // the Subtitle Style page: outside the Option 1..6 cycle (LP_N), slot 0
static volatile LONG g_subReq;     // 1 = open the Subtitle Style page, 2 = back to Option 3 (3 / 4: Screen Effects)
static int g_subMenu;              // Option 3's Subtitles row opens the page
// v3.16.0 (staging/screen_effects_v2/RE_fade.md): the hidden pages open and close through the stock sub-screen exit
// (row return 4 -> fade out -> Options state 5, whose call 0x526CD2 is SubScreen -> fade in), like Button Config. 0 =
// those hooks are not in place: the v3.15.0 slide (row return 1, Mode2Slide) is used instead.
static int g_subScreenOn;
static void(__cdecl *g_subTick)(void);  // the hidden test trigger, run by the 0x4EF982 dispatcher (quicksave.inc)

// v3.15.0: Screen Effects (screenfx.inc; staging/screen_effects/PLAN.md), set on the Screen Effects page, a hidden
// logical page (LP_FX) opened by Enter on Option 3's stock Noise Effect row. Noise Strength scales the level of the
// game's two grain calls (0x432FBE / 0x432F9E -> 0x567040).
// v3.16.0 (staging/screen_effects_v2/PLAN.md): Noise Type replaces the game's grain grid with an own layer; Filter
// Strength is gone (fixed parameters per filter, screenfx.inc); the filter is drawn over the scene at 0x4EFBFD.
// v3.18.0 (staging/screen_effects_v3/PLAN.md): + Noise Colour (N1); Noise Type loses Colour, gains Scan Roll / Rain (N2);
// Noir removed (N6; "Noir" reads as Night Vision Mono, "Colour" as Fine + Full Colour)
enum { FX_NOISE, FX_NTYPE, FX_NCOLOUR, FX_FILTER, FX_CUT, FX_N };  // v3.22.0: + Cut-scenes (user item 17)
#define FX_PP_ROWS 3  // v3.43 PP343: Bloom, Colour Grading, Otherworld Lens after Cut-scenes (post.inc; their own text block)
#define FX_DOF_ROWS 2  // v3.44 DF344: Depth of Field, DoF Strength after the post rows (dof.inc DofBuildRows; the MSG_DOF0 block)
#define FX_VMAX 11
#define FX_FILTERS 10
static const int kFxCount[FX_N] = { 4, 7, 8, FX_FILTERS, 2 };
static const char *const kFxIniKey[FX_N] = { "NoiseStrength", "NoiseType", "NoiseColour", "Filter", "Cutscenes" };
static const char *const kFxName[FX_N][FX_VMAX] = {
    { "Low", "Normal", "High", "Very High" },
    { "Stock", "Fine", "Coarse", "Film", "VHS", "Scan Roll", "Rain" },
    { "Stock", "Mono", "Full Colour", "Red", "Green", "Blue", "Amber", "Sepia" },
    { "None", "CRT", "Vivid", "Sepia", "Old Film", "Night Vision", "Night Vision Mono", "Thermal", "Spirit Vision", "Vignette" },
    { "On", "Off" },  // v3.22.0: the filter in real-time cut-scenes
};
static const int kFxStock[FX_N] = { 1, 0, 0, 0, 0 };  // Normal, Stock, Stock, None, On
static volatile LONG g_fx[FX_N] = { 1, 0, 0, 0, 0 };
static LONG g_snapFx[FX_N];
static int g_fxOn;                 // the three core calls are patched (all or nothing)
static int g_fxMenu;               // Option 3's Noise Effect row opens the page
#define LP_FX 7                    // the Screen Effects page: outside the cycle, slot 1 (7 % 3; Option 3 is slot 2)
// v3.25.0 (staging/feedback_v325/PLAN.md 4.1): Option 5's hidden pages, in slots other than Option 5's (slot 1); lp 10 / 13
// would be slot 1 too. Their requests: REQ_OPEN | lp opens lp from its parent, REQ_BACK returns to the parent.
#define LP_PLAYER 8                // slot 2
#define LP_ENEMY 9                 // slot 0
#define LP_SPECIAL 11              // v3.27.0: Option 5 > Special (slot 2; its parent Option 5 is slot 1)
#define LP_SHD 10                  // v3.40 SH340: Option 3 > Realtime Shadow > Shadows (slot 1; its parent Option 3 is slot 2)
// (10: v3.33 CAM333's Option 6 > Camera page; v3.37 CP337: its settings moved to the pause menu, the page is gone)
#define LP_DOF 12                  // v3.43 DF343: Option 3 > Hide Icon > Depth of Field (slot 0; its parent Option 3 is slot 2)
#define LP_RANDO 14                // v3.48 RZ348: Option 5 > Randomizer (slot 2; its parent Option 5 is slot 1; 13 would be slot 1)
#define LP_MAX 15                  // the page arrays (v3.43 DF343: + LP_DOF; was 12)  // v3.48 RZ348: 13 -> 15 (+ LP_RANDO)
#define REQ_OPEN 0x100
#define REQ_BACK 0x200
// review FX-002 / FX-003: the preview gate. The post-pass wrapper counts its calls (g_fxFrame, incremented at the END
// of each call); the page's row-0 draw stores the count (g_fxStamp); the preview applies only when they are equal (the
// page was drawn in this same frame). -1 = never drawn (set again when Options opens); the counter skips -1.
static volatile LONG g_fxFrame = 0, g_fxStamp = -1;

// v3.14.0: vibration strength (staging/vibration/PLAN.md). Option 2's Vibration row: Off / Low / Medium / High; Off =
// the game's cfg+0x0B = 0, Low = the game's own strength. GOG's dinput8.dll emulates the game's constant-force effect
// with SendForceFeedbackCommand(0x80000000 | v << 8 | v), v = 32 / 33, every 100 ms (vtable slot 22 of its device);
// Medium / High scale those motor bytes.
static volatile LONG g_vibLevel = 1;   // 1 Low, 2 Medium, 3 High
static LONG g_snapVib;
static int g_vibMenu;                  // Option 2's Vibration row is ours
static int g_vibOn;                    // the slot-22 hook is placed on new devices
static volatile LONG g_vibCap = 255;   // per motor byte; 100 for an Xbox pad over HID (RE 2.2 [inferred])
static volatile LONG g_vibLogN;        // commands still to log after a level change
static int g_vibTest;
static void(__cdecl *g_vibTick)(void);
static void(__cdecl *g_bldTick)(void);  // v3.25.0: the Blood Colour fallback (gameplay.inc BldSanitize), run by the 0x4EF982 dispatcher
static const char *const kVibName[4] = { "Off", "Low", "Medium", "High" };
static int VibChanged(void);
static LONG VibCapNow(void);  // after buttoncfg.inc (pad family)
static void SubPreview(void);
static const char *const kUnknownText = "Not used by this mod. Kept as you had it.";

static int g_enable = 1, g_width, g_height, g_resizable = 1, g_keepActive = 1, g_capture = 1, g_snap = 1;
static int g_saveTitles = 1;  // v3.5.2: [Saves] WorldTitles
// v3.7.0: [ExtraMenu]: Enabled (the pause Extra Menu at all) and one switch per row, in this order
enum { EM_CHEST, EM_SAVE, EM_LOAD, EM_QSAVE, EM_QLOAD, EM_EILEEN, EM_N };
static const char *const kEmKey[EM_N] = { "Chest", "Save", "Load", "QuickSave", "QuickLoad", "Eileen" };
static int g_emEnabled = 1;
static volatile LONG g_emOn[EM_N] = { 1, 1, 1, 1, 1, 1 };
// v3.8.0: [SaveBanks] (savebanks.inc): banks on / off and the bank the save and load lists open on (1 = sh4.sav)
#define SB_MAX 99
static volatile LONG g_sbOn = 1, g_sbBank = 1;
static int g_sbInstalled;
// v3.9.0: [EileenOutfit] (eileenoutfit.inc): the companion Eileen's outfit engine (the table's Eileen Outfit v4)
static int g_eoEnabled = 1, g_eoIniOutfit = -1, g_eoIniAuto = 1, g_eoIniEink = 1;
// v3.7.0: SH4R_Extra (export 197): the cheat table reads `owns` (bit 0 = Refined draws the pause Extra Menu, so its
// Pause Menu+ stays off) and its Eileen Outfit writes the address of its eo_api block into `eoApi` (0 when off)
volatile DWORD SH4R_Extra[4] = { 0x58524853 /* 'SHRX' */, 1, 0, 0 };
static int g_noMinimize = 1, g_hidFix = 1, g_aspectMenu = 1, g_fitWrapper = 1, g_menus43Ini = 1, g_borderlessIni;
static int g_menus43Log;  // v1.6: [Window] Menus43Log
static int g_menuPillars = 1;  // v1.8: [Window] MenuPillars
static int g_view43 = 1;       // v3.41 VW341: hidden [Window] WindowView43 (Room 302's window views at 4:3)
static int g_edgeLog = 1;      // v3.41 VW341: hidden [Window] EdgeCoverLog (Borderless: what covers the game's edges)
static int g_pixAlign = 1;     // v3.42 EG342: hidden [Window] PixelAlign2D (2D canvases on pixel edges, pillars past the edges)
static int g_fovIni;           // v2.0: [Window] FOV (degrees, 0 = the game's own; v1.9 had FieldOfView in percent)
static int g_fovFpIni;         // v3.3: [Window] FOVFirstPerson (Room 302's first-person camera; FOV is the rest)
static volatile LONG g_sparkleOn;  // v3.3: [Items] Sparkle (items.inc); v3.3.5: the keys are [Items] Shine*
static int g_spMemos, g_spRange, g_spWalls;  // v3.3: [Items] SparkleMemos / SparkleRange / SparkleThroughWalls
static int g_spDepth;                        // v3.3.2: [Items] SparkleDepthTest (v3.3.3: experimental, off)
static int g_spBodies = 1;                   // v3.3.3: [Items] SparkleBodies
static float g_spEvery = 7.0f, g_spFor = 2.0f;  // v3.3.2: [Items] SparkleEvery / SparkleFor (seconds)
static int g_fpsIni = 30;      // v1.9: [Window] FrameRate
static int g_fpsHigh;          // v3.31 FPS331: hidden [Window] HighFrameRates=1 shows 120 / 180 (experimental); 0 = 30 / 60 only
static int g_gateIni = 1, g_compGateIni = 0, g_smoothIni = 1;  // v2.1: [Window] EntityGate / CompanionGate / EntitySmooth
static int g_frameMode = 0;    // [Window] FrameMode (v3.0 default 0 = true frame rate; 1 = v2.1 interpolation, the logic stays at 30)
static int g_rateProbe;        // v3.0: hidden [Window] RateProbe (FrameMode 0): loop at 30 ticks/s, engine told FrameRate
static int g_interpAutoLower = 1;  // v2.1: [Window] InterpAutoLower
static int g_smoothCut;          // v2.1: [Window] SmoothCutscenes (FrameMode 1)
static volatile int g_cliW, g_cliH;  // v1.3: the client size the window is placed at (the ini size, then the picture's shape)

static void Put(FILE *f, const char *s) { fputs(s, f); fputs("\r\n", f); }

static void PutKey(FILE *f, const char *key, const char *text, const char *val)
{
    fprintf(f, "; %-*s%s\r\n%s=%s\r\n", KEY_COL, key, text, key, val);
}

// Writes the settings file with vals[k] for kIniKeys[k]; extra = "key=value\0key=value\0\0" of [Window] keys this mod
// does not know (kept from the source file), or NULL.
static int WriteIni(const char *path, char vals[][64], const char *extra)
{
    char tmp[MAX_PATH];
    FILE *f = NULL;
    int s, k, ok;
    sprintf_s(tmp, sizeof tmp, "%s.tmp", path);
    if (fopen_s(&f, tmp, "wb") || !f) return 0;
    for (k = 0; k < (int)(sizeof kIniHeader / sizeof kIniHeader[0]); ++k) {
        fputs("; ", f);
        Put(f, kIniHeader[k]);
    }
    for (s = 0; s < (int)(sizeof kIniSections / sizeof kIniSections[0]); ++s) {
        const char *c;
        fputs("\r\n; ", f);
        for (c = kIniSections[s].title; *c; ++c) fputc(*c >= 'a' && *c <= 'z' ? *c - 32 : *c, f);
        fputs("\r\n", f);
        for (k = 0; k < 3 && kIniSections[s].about[k]; ++k) {
            fputs("; ", f);
            Put(f, kIniSections[s].about[k]);
        }
        fprintf(f, "[%s]\r\n", kIniSections[s].name);
        for (k = 0; k < NKEYS; ++k)
            if (!strcmp(kIniKeys[k].section, kIniSections[s].name)) PutKey(f, kIniKeys[k].key, kIniKeys[k].text, vals[k]);
        if (!strcmp(kIniSections[s].name, "Window") && extra) {
            const char *e;
            for (e = extra; *e; e += strlen(e) + 1) {
                char key[128];
                const char *eq = strchr(e, '=');
                size_t n = eq ? (size_t)(eq - e) : strlen(e);
                if (n >= sizeof key) n = sizeof key - 1;
                memcpy(key, e, n);
                key[n] = 0;
                PutKey(f, key, kUnknownText, eq ? eq + 1 : "");
            }
        }
    }
    ok = !ferror(f);
    if (fclose(f)) ok = 0;
    if (!ok || !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp);
        return 0;
    }
    return 1;
}

static int KnownWindowKey(const char *key)
{
    int k;
    for (k = 0; k < NKEYS; ++k)
        if (!strcmp(kIniKeys[k].section, "Window") && !_stricmp(kIniKeys[k].key, key)) return 1;
    return 0;
}

// First start: SH4Refined.ini is missing (and no SH4WindowMod.ini to rename). Its values come from SH4PlayableEileen.ini [Window] (the plugin's settings
// up to v0.81, every key kept, unknown ones too), else from the laptop rig's scripts\SH4Windowed.ini, else defaults.
// The Playable Eileen plugin removes its own [Window] section only after this file holds every key of it.
static char g_vals[NKEYS][64];  // the values CreateIni found, used when the file could not be written
static int g_iniFallback;
static int g_iniCreated;  // v3.33 CAM333: CreateIni wrote SH4Refined.ini in this start (camera.inc imports PE's [Camera])

static void CreateIni(void)
{
    char (*vals)[64] = g_vals, pe[MAX_PATH], asi[MAX_PATH], sec[8192], extra[8192];
    const char *from = "defaults";
    char *x = extra, *e;
    int k, havePe, haveAsi;
    sprintf_s(pe, sizeof pe, "%sSH4PlayableEileen.ini", g_dir);
    sprintf_s(asi, sizeof asi, "%sscripts\\SH4Windowed.ini", g_dir);
    havePe = GetFileAttributesA(pe) != INVALID_FILE_ATTRIBUTES && GetPrivateProfileSectionA("Window", sec, sizeof sec, pe) > 0;
    haveAsi = !havePe && GetFileAttributesA(asi) != INVALID_FILE_ATTRIBUTES &&
              GetPrivateProfileSectionA("Window", sec, sizeof sec, asi) > 0;
    for (k = 0; k < NKEYS; ++k) {
        strcpy_s(vals[k], 64, kIniKeys[k].def);
        if (havePe) {
            GetPrivateProfileStringA(kIniKeys[k].section, kIniKeys[k].key, kIniKeys[k].def, vals[k], 64, pe);
        } else if (haveAsi && !strcmp(kIniKeys[k].section, "Window")) {
            // SH4Windowed.asi: same keys, except ConfineMouse (its CaptureMouse); Enable / NoMinimize are not there
            const char *key = !strcmp(kIniKeys[k].key, "CaptureMouse") ? "ConfineMouse" : kIniKeys[k].key;
            GetPrivateProfileStringA("Window", key, kIniKeys[k].def, vals[k], 64, asi);
        }
    }
    extra[0] = extra[1] = 0;
    if (havePe) {  // keys of the plugin's [Window] this mod does not know: kept, so nothing the user wrote is lost
        from = "SH4PlayableEileen.ini [Window]";
        for (e = sec; *e; e += strlen(e) + 1) {
            char key[128];
            const char *eq = strchr(e, '=');
            size_t n = eq ? (size_t)(eq - e) : strlen(e), len = strlen(e) + 1;
            if (n >= sizeof key || *e == ';') continue;
            memcpy(key, e, n);
            key[n] = 0;
            if (KnownWindowKey(key) || (size_t)(x - extra) + len + 1 >= sizeof extra) continue;
            memcpy(x, e, len);
            x += len;
            *x = 0;
        }
    } else if (haveAsi) {
        from = "scripts\\SH4Windowed.ini";
    }
    if (WriteIni(g_iniPath, vals, extra[0] ? extra : NULL)) {
        g_iniCreated = 1;  // v3.33 CAM333
        Log("settings: SH4Refined.ini was missing; written with the values of %s", from);
    } else {
        g_iniFallback = 1;  // this start still uses them; nothing else is written
        Log("settings: could not write SH4Refined.ini (error %lu); using the values of %s for this start", GetLastError(), from);
    }
}

static int IniInt(const char *section, const char *key)
{
    int k;
    for (k = 0; k < NKEYS; ++k)
        if (!strcmp(kIniKeys[k].section, section) && !strcmp(kIniKeys[k].key, key)) {
            char buf[64];
            if (g_iniFallback) return atoi(g_vals[k]);
            if (!GetPrivateProfileStringA(section, key, "", buf, sizeof buf, g_iniPath)) {
                // a key added in a later version: append it with its default (no comment line)
                WritePrivateProfileStringA(section, key, kIniKeys[k].def, g_iniPath);
                Log("settings: [%s] %s was missing, added as %s", section, key, kIniKeys[k].def);
                return atoi(kIniKeys[k].def);
            }
            return atoi(buf);
        }
    return 0;
}

// v1.7: a text value (a key added in a later version is appended with its default, like IniInt)
static void IniStr(const char *section, const char *key, char *out, DWORD n)
{
    int k;
    out[0] = 0;
    for (k = 0; k < NKEYS; ++k)
        if (!strcmp(kIniKeys[k].section, section) && !strcmp(kIniKeys[k].key, key)) {
            if (g_iniFallback) {
                strcpy_s(out, n, g_vals[k]);
                return;
            }
            GetPrivateProfileStringA(section, key, "\x01", out, n, g_iniPath);
            if (!strcmp(out, "\x01")) {
                WritePrivateProfileStringA(section, key, kIniKeys[k].def, g_iniPath);
                Log("settings: [%s] %s was missing, added as \"%s\"", section, key, kIniKeys[k].def);
                strcpy_s(out, n, kIniKeys[k].def);
            }
            return;
        }
}

static char g_monIni[MAX_PATH], g_monNameIni[128], g_monPathIni[MAX_PATH];  // v1.7: [Window] Monitor / MonitorName / MonitorPath
static int g_autoSize;  // Width / Height 0: the size follows the chosen monitor (v1.7; the primary before)

#define FOV_REF_V 1.0995574f  // 63 deg: the vertical angle of most rooms' third-person camera records (1.10 rad)
#define FOV_REF_FP_V 0.9599311f  // v3.3: 55 deg, Room 302's first-person camera (0x3F75BE0C at 0x5007EF / 0x500B75)
// v2.0: v1.9 kept the field of view as a factor in percent ([Window] FieldOfView, 100 = the game's own). It becomes
// [Window] FOV in degrees: the reference camera's horizontal angle at 16:9 with that factor, to the nearest 5.
static void FovMigrate(void)
{
    char buf[32];
    int pct, deg = 0;
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Window", "FOV", "\x01", buf, sizeof buf, g_iniPath);
    if (strcmp(buf, "\x01")) return;  // already there
    GetPrivateProfileStringA("Window", "FieldOfView", "\x01", buf, sizeof buf, g_iniPath);
    if (!strcmp(buf, "\x01")) return;  // neither: IniInt adds FOV=0
    pct = atoi(buf);
    if (pct > 0 && pct != 100) {
        const double h = 2.0 * atan((pct / 100.0) * tan(0.5 * FOV_REF_V) * 16.0 / 9.0) * 57.29577951;
        deg = (int)(h / 5.0 + 0.5) * 5;
        if (deg < 60) deg = 60;
        if (deg > 120) deg = 120;
    }
    sprintf_s(buf, sizeof buf, "%d", deg);
    WritePrivateProfileStringA("Window", "FOV", buf, g_iniPath);
    WritePrivateProfileStringA("Window", "FieldOfView", NULL, g_iniPath);
    Log("settings: [Window] FieldOfView=%d (percent, v1.9) is now FOV=%d (degrees, 0 = the game's own)", pct, deg);
}

// v3.3 (user: "Split Field of View option into FOV first person and FOV third person"): until v3.2 one FOV row
// scaled both cameras. FOVFirstPerson starts as a copy of FOV, so nothing changes until the player picks a new angle.
static void FovFpMigrate(void)
{
    char buf[32];
    if (g_iniFallback) return;
    GetPrivateProfileStringA("Window", "FOVFirstPerson", "\x01", buf, sizeof buf, g_iniPath);
    if (strcmp(buf, "\x01")) return;  // already there
    GetPrivateProfileStringA("Window", "FOV", "0", buf, sizeof buf, g_iniPath);
    sprintf_s(buf, sizeof buf, "%d", atoi(buf));
    WritePrivateProfileStringA("Window", "FOVFirstPerson", buf, g_iniPath);
    Log("settings: [Window] FOVFirstPerson=%s (copied from FOV; the two are separate from v3.3)", buf);
}

// v3.3.5 (user: "change the selectable values to 100..120 in steps of 5 plus Default; drop 90 and below"): FOV First
// Person offers Default, 100, 105, 110, 115 and 120. A saved angle outside that is moved once and written back: below
// 95 (narrower than, or about, the game's own first-person view, 85 deg wide at 16:9) -> 0, the game's own; 95..120 ->
// the nearest step (95 -> 100); above 120 -> 120.
static int FovFpAllowedDeg(int deg)
{
    if (deg < 95) return 0;
    if (deg >= 120) return 120;
    return deg <= 100 ? 100 : ((deg + 2) / 5) * 5;
}
static void FovFpRangeMigrate(void)
{
    const int to = FovFpAllowedDeg(g_fovFpIni);
    char buf[16];
    if (to == g_fovFpIni) return;
    Log("settings: [Window] FOVFirstPerson=%d is not offered any more (Default, 100 to 120): now %d%s", g_fovFpIni, to,
        to ? " degrees" : " (the game's own)");
    g_fovFpIni = to;
    if (g_iniFallback) return;
    sprintf_s(buf, sizeof buf, "%d", to);
    WritePrivateProfileStringA("Window", "FOVFirstPerson", buf, g_iniPath);
}

// v3.3.5 (user: rename "Item Sparkle" to "Item Shine", keep the old ini key working): v3.3.0-v3.3.4 wrote [Items]
// Sparkle, SparkleMemos, ... An old key found in the file is taken over by its Shine* name and removed. SpIniPoll runs
// this again when the file changes, so an old key typed in while the game runs works too. The old key wins over the
// new one: it can only be there because it was written later (by hand, or by an older build after a rollback).
static const char *const kShineOld[] = { "Sparkle", "SparkleMemos", "SparkleRange", "SparkleThroughWalls",
                                         "SparkleBodies", "SparkleDepthTest", "SparkleEvery", "SparkleFor" };
static const char *const kShineNew[] = { "Shine", "ShineMemos", "ShineRange", "ShineThroughWalls", "ShineBodies",
                                         "ShineDepthTest", "ShineEvery", "ShineFor" };
static int ShineMigrate(void)
{
    char buf[64];
    int k, n = 0;
    if (g_iniFallback) return 0;
    for (k = 0; k < (int)(sizeof kShineOld / sizeof kShineOld[0]); ++k) {
        GetPrivateProfileStringA("Items", kShineOld[k], "\x01", buf, sizeof buf, g_iniPath);
        if (!strcmp(buf, "\x01")) continue;
        WritePrivateProfileStringA("Items", kShineNew[k], buf, g_iniPath);
        WritePrivateProfileStringA("Items", kShineOld[k], NULL, g_iniPath);
        Log("settings: [Items] %s=%s is now %s=%s (Item Sparkle is called Item Shine from v3.3.5)", kShineOld[k], buf,
            kShineNew[k], buf);
        ++n;
    }
    return n;
}

static void ReadIni(void)
{
    if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES) CreateIni();
    g_enable     = IniInt("Window", "Enable");
    g_width      = IniInt("Window", "Width");
    g_height     = IniInt("Window", "Height");
    g_resizable  = IniInt("Window", "Resizable");
    g_keepActive = IniInt("Window", "KeepActiveOnAltTab");
    g_capture    = IniInt("Window", "CaptureMouse");
    g_snap       = IniInt("Window", "SnapToStandard");
    g_noMinimize = IniInt("Window", "NoMinimize");
    g_aspectMenu = IniInt("Window", "AspectMenu");
    g_fitWrapper = IniInt("Window", "FitWrapper");
    {   // v3.39 PR339: the v3.12.0 [Graphics] rows are gone; a value that is not Default is named once (dxcfg.ini decides)
        static const char *const kDx[3][2] = { { "general", "vsync" }, { "enhancements", "antialiasing" }, { "enhancements", "anisotropic" } };
        static const char *const kWhat[3] = { "V-Sync", "anti-aliasing", "anisotropic filtering" };
        char dx[MAX_PATH];
        int r;
        sprintf_s(dx, sizeof dx, "%sdxcfg.ini", g_dir);
        for (r = 0; r < 3; ++r) {
            char buf[32], now[32];
            if (g_iniFallback) break;
            GetPrivateProfileStringA("Graphics", kGfxIniKey[r], "", buf, sizeof buf, g_iniPath);
            if (!buf[0] || !_stricmp(buf, "Default")) continue;
            GetPrivateProfileStringA(kDx[r][0], kDx[r][1], "(not set)", now, sizeof now, dx);
            Log("settings: [Graphics] %s=%s is not used any more: %s is now set in GOG's DirectX Settings (dxcfg.exe; GOG Galaxy > "
                "Manage > DirectX Settings); dxcfg.ini says %s=%s", kGfxIniKey[r], buf, kWhat[r], kDx[r][1], now);
        }
    }
    g_menus43Ini = IniInt("Window", "Menus43");
    g_menus43Log = IniInt("Window", "Menus43Log");
    g_menuPillars = IniInt("Window", "MenuPillars");
    g_view43 = GetPrivateProfileIntA("Window", "WindowView43", 1, g_iniPath) ? 1 : 0;  // v3.41 VW341 (hidden)
    g_edgeLog = GetPrivateProfileIntA("Window", "EdgeCoverLog", 1, g_iniPath) ? 1 : 0;  // v3.41 VW341 (hidden)
    g_pixAlign = GetPrivateProfileIntA("Window", "PixelAlign2D", 1, g_iniPath) ? 1 : 0;  // v3.42 EG342 (hidden)
    g_borderlessIni = IniInt("Window", "Borderless");
    FovMigrate();  // v2.0: v1.9's FieldOfView (percent) becomes FOV (degrees)
    g_fovIni     = IniInt("Window", "FOV");
    FovFpMigrate();  // v3.3: the first-person row starts from the old single value
    g_fovFpIni   = IniInt("Window", "FOVFirstPerson");
    FovFpRangeMigrate();  // v3.3.5: the first-person row offers Default and 100-120
    ShineMigrate();       // v3.3.5: [Items] Sparkle* -> Shine*, before IniInt adds missing Shine* keys with defaults
    g_sparkleOn  = IniInt("Items", "Shine") ? 1 : 0;  // v3.3
    g_spMemos    = IniInt("Items", "ShineMemos") ? 1 : 0;
    g_spRange    = IniInt("Items", "ShineRange");
    if (g_spRange < 500) g_spRange = 500;
    if (g_spRange > 20000) g_spRange = 20000;
    g_spWalls    = IniInt("Items", "ShineThroughWalls") ? 1 : 0;
    g_spDepth    = IniInt("Items", "ShineDepthTest") ? 1 : 0;  // v3.3.2
    g_spBodies   = IniInt("Items", "ShineBodies") ? 1 : 0;     // v3.3.3
    {
        char b[32];
        IniStr("Items", "ShineEvery", b, sizeof b);
        g_spEvery = (float)atof(b);
        IniStr("Items", "ShineFor", b, sizeof b);
        g_spFor = (float)atof(b);
        if (!(g_spEvery >= 2.0f)) g_spEvery = 2.0f;
        if (g_spEvery > 60.0f) g_spEvery = 60.0f;
        if (!(g_spFor >= 0.5f)) g_spFor = 0.5f;
        if (g_spFor > g_spEvery) g_spFor = g_spEvery;
    }
    g_fpsIni     = IniInt("Window", "FrameRate");
    // v3.31 FPS331 (the user, 2026-11-18): 120 and 180 are hidden unless [Window] HighFrameRates=1. A saved 120 / 180 runs
    // as 60; the ini keeps the user's value (the Options row writes it only when changed), so the key brings it back
    g_fpsHigh    = GetPrivateProfileIntA("Window", "HighFrameRates", 0, g_iniPath) ? 1 : 0;
    if (!g_fpsHigh && g_fpsIni > 60) {
        Log("frame rate: [Window] FrameRate=%d needs the hidden HighFrameRates=1 (experimental): the game runs at 60", g_fpsIni);
        g_fpsIni = 60;
    }
    g_gateIni    = IniInt("Window", "EntityGate");     // v2.1
    g_compGateIni = IniInt("Window", "CompanionGate");  // v2.1
    g_smoothIni  = IniInt("Window", "EntitySmooth");   // v2.1
    g_frameMode  = IniInt("Window", "FrameMode") ? 1 : 0;
    // v3.0 hidden diagnostic (FrameMode 0): the game loop keeps 30 ticks a second while the engine is told FrameRate,
    // so everything that follows the rate runs in slow motion and every per-tick leftover still runs at normal speed
    g_rateProbe  = GetPrivateProfileIntA("Window", "RateProbe", 0, g_iniPath) ? 1 : 0;
    g_interpAutoLower = IniInt("Window", "InterpAutoLower") ? 1 : 0;
    g_smoothCut  = IniInt("Window", "SmoothCutscenes") ? 1 : 0;  // v2.1
    g_hidFix     = IniInt("Input", "ControllerCrashFix");
    g_saveTitles = IniInt("Saves", "WorldTitles");  // v3.5.2
    g_emEnabled = IniInt("ExtraMenu", "Enabled");  // v3.7.0
    {
        int k;
        for (k = 0; k < EM_N; ++k) g_emOn[k] = IniInt("ExtraMenu", kEmKey[k]) ? 1 : 0;
    }
    g_sbOn = IniInt("SaveBanks", "Enabled") ? 1 : 0;  // v3.8.0
    g_eoEnabled = IniInt("EileenOutfit", "Enabled");  // v3.9.0
    g_eoIniOutfit = IniInt("EileenOutfit", "Outfit");
    if (g_eoIniOutfit < -1 || g_eoIniOutfit > 1) g_eoIniOutfit = -1;
    g_eoIniAuto = IniInt("EileenOutfit", "Instant") ? 1 : 0;
    g_eoIniEink = IniInt("EileenOutfit", "Cutscenes");
    if (g_eoIniEink < 0 || g_eoIniEink > 2) g_eoIniEink = 1;
    g_sbBank = IniInt("SaveBanks", "Bank");
    if (g_sbBank < 1 || g_sbBank > SB_MAX) g_sbBank = 1;
    IniStr("Window", "Monitor", g_monIni, sizeof g_monIni);
    IniStr("Window", "MonitorName", g_monNameIni, sizeof g_monNameIni);
    IniStr("Window", "MonitorPath", g_monPathIni, sizeof g_monPathIni);
    if (g_width <= 0 || g_height <= 0) {
        g_autoSize = 1;  // MonInit takes the chosen monitor's height instead
        g_height = GetSystemMetrics(SM_CYSCREEN) * 3 / 4;
        g_width = g_height * 4 / 3;
    }
    g_cliW = g_width;
    g_cliH = g_height;
}

// ================================================================ v1.7: monitor
// User (2026-11-01): "give an option to switch monitors like what a few modern games now do", remembered "between game
// and pc reboots". The attached monitors are listed once at start (EnumDisplayMonitors), in the order of their GDI
// number (\\.\DISPLAYn, the number Windows' "Identify" shows); Options > Monitor names them "Display n". A monitor is
// remembered by three things, tried in this order at the next start: its device interface path (MonitorPath,
// EnumDisplayDevices with EDD_GET_DEVICE_INTERFACE_NAME: EDID maker / product code plus the connector, stable across
// reboots), its EDID name (MonitorName, DisplayConfigGetDeviceInfo; only when exactly one attached monitor has it, so it
// finds a monitor moved to another port but not one of two identical ones) and its GDI name (Monitor=\\.\DISPLAYn). A
// plain number typed by hand (Monitor=2) means \\.\DISPLAY2 and skips the other two; 0 or empty is the primary monitor.
// A remembered monitor that is not attached falls back to the primary one (the ini keeps it for the next start).
// Coordinates come from GetMonitorInfo (the window's own DPI view); the GOG wrapper's desktop size from
// EnumDisplaySettings (pixels, as the wrapper reads it itself).
#define MON_MAX 16
typedef struct {
    char dev[CCHDEVICENAME];  // \\.\DISPLAYn
    char name[128];           // EDID name (DisplayConfig), else the monitor's device string
    char path[MAX_PATH];      // monitor device interface path
    int num;                  // n of \\.\DISPLAYn (0 if none)
    int primary;
    RECT rc;                  // at start (the live rect is looked up by dev each time: MonGet)
} MonEntry;
static MonEntry g_mons[MON_MAX];
static int g_monN;
static volatile LONG g_monSel;          // index into g_mons of the chosen monitor
static volatile LONG g_monPlaced = -1;  // the monitor the window was last placed on (Watch thread)

static BOOL CALLBACK MonEnumProc(HMONITOR h, HDC dc, LPRECT r, LPARAM lp)
{
    MONITORINFOEXA mi;
    MonEntry *m;
    (void)dc; (void)r; (void)lp;
    if (g_monN >= MON_MAX) return FALSE;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoA(h, (MONITORINFO *)&mi)) return TRUE;
    m = &g_mons[g_monN++];
    memset(m, 0, sizeof *m);
    strcpy_s(m->dev, sizeof m->dev, mi.szDevice);
    m->primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    m->rc = mi.rcMonitor;
    if (!_strnicmp(mi.szDevice, "\\\\.\\DISPLAY", 11)) m->num = atoi(mi.szDevice + 11);
    return TRUE;
}

// The EDID names of the active monitors, by GDI source name (user32 DisplayConfig, Windows 7+, looked up at run time)
typedef LONG(WINAPI *GetDCBufSizes_t)(UINT32, UINT32 *, UINT32 *);
typedef LONG(WINAPI *QueryDC_t)(UINT32, UINT32 *, DISPLAYCONFIG_PATH_INFO *, UINT32 *, DISPLAYCONFIG_MODE_INFO *, DISPLAYCONFIG_TOPOLOGY_ID *);
typedef LONG(WINAPI *DCGetInfo_t)(DISPLAYCONFIG_DEVICE_INFO_HEADER *);
static void MonEdidNames(void)
{
    HMODULE u = GetModuleHandleA("user32.dll");
    GetDCBufSizes_t sizes = u ? (GetDCBufSizes_t)GetProcAddress(u, "GetDisplayConfigBufferSizes") : NULL;
    QueryDC_t query = u ? (QueryDC_t)GetProcAddress(u, "QueryDisplayConfig") : NULL;
    DCGetInfo_t info = u ? (DCGetInfo_t)GetProcAddress(u, "DisplayConfigGetDeviceInfo") : NULL;
    UINT32 np = 0, nm = 0, i;
    DISPLAYCONFIG_PATH_INFO *paths;
    DISPLAYCONFIG_MODE_INFO *modes;
    if (!sizes || !query || !info || sizes(QDC_ONLY_ACTIVE_PATHS, &np, &nm) != ERROR_SUCCESS || !np || np > 64 || nm > 128) return;
    paths = (DISPLAYCONFIG_PATH_INFO *)calloc(np, sizeof *paths);
    modes = (DISPLAYCONFIG_MODE_INFO *)calloc(nm ? nm : 1, sizeof *modes);
    if (paths && modes && query(QDC_ONLY_ACTIVE_PATHS, &np, paths, &nm, modes, NULL) == ERROR_SUCCESS) {
        for (i = 0; i < np; ++i) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME src;
            DISPLAYCONFIG_TARGET_DEVICE_NAME tgt;
            char gdi[CCHDEVICENAME], nm8[128];
            int k;
            memset(&src, 0, sizeof src);
            src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            src.header.size = sizeof src;
            src.header.adapterId = paths[i].sourceInfo.adapterId;
            src.header.id = paths[i].sourceInfo.id;
            memset(&tgt, 0, sizeof tgt);
            tgt.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            tgt.header.size = sizeof tgt;
            tgt.header.adapterId = paths[i].targetInfo.adapterId;
            tgt.header.id = paths[i].targetInfo.id;
            if (info(&src.header) != ERROR_SUCCESS || info(&tgt.header) != ERROR_SUCCESS || !tgt.monitorFriendlyDeviceName[0]) continue;
            WideCharToMultiByte(CP_ACP, 0, src.viewGdiDeviceName, -1, gdi, sizeof gdi, NULL, NULL);
            WideCharToMultiByte(CP_ACP, 0, tgt.monitorFriendlyDeviceName, -1, nm8, sizeof nm8, NULL, NULL);
            for (k = 0; k < g_monN; ++k)
                if (!_stricmp(g_mons[k].dev, gdi)) strcpy_s(g_mons[k].name, sizeof g_mons[k].name, nm8);
        }
    }
    free(paths);
    free(modes);
}

static int MonCmp(const void *a, const void *b)
{
    // v3.2.2: by desktop position (left to right, then top to bottom), so "Display 1, 2" read like the desk; the GDI
    // number (\\.\DISPLAYn) is often not 1..N (user, two screens: DISPLAY2 and DISPLAY3, shown as "Display 3")
    // v3.19.0 (user item 5): the primary first, then the Windows GDI number (\.\DISPLAYn), named by rank (MonLabel)
    const MonEntry *x = (const MonEntry *)a, *y = (const MonEntry *)b;
    int nx = x->num ? x->num : 1000, ny = y->num ? y->num : 1000;
    if (x->primary != y->primary) return x->primary ? -1 : 1;
    return nx != ny ? nx - ny : _stricmp(x->dev, y->dev);
}

static int MonPrimary(void)
{
    int k;
    for (k = 0; k < g_monN; ++k)
        if (g_mons[k].primary) return k;
    return 0;
}

typedef struct { const char *dev; HMONITOR h; } MonFind;
static BOOL CALLBACK MonFindProc(HMONITOR h, HDC dc, LPRECT r, LPARAM lp)
{
    MonFind *f = (MonFind *)lp;
    MONITORINFOEXA mi;
    (void)dc; (void)r;
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(h, (MONITORINFO *)&mi) && !_stricmp(mi.szDevice, f->dev)) {
        f->h = h;
        return FALSE;
    }
    return TRUE;
}

// Monitor k now: its handle and info, found again by its GDI name (handles change when the display setup changes);
// the primary monitor if it is gone.
static HMONITOR MonGet(int k, MONITORINFOEXA *out)
{
    MonFind f;
    HMONITOR h = NULL;
    f.dev = k >= 0 && k < g_monN ? g_mons[k].dev : "";
    f.h = NULL;
    if (f.dev[0]) {
        EnumDisplayMonitors(NULL, NULL, MonFindProc, (LPARAM)&f);
        h = f.h;
    }
    if (!h) {
        POINT o = { 0, 0 };
        h = MonitorFromPoint(o, MONITOR_DEFAULTTOPRIMARY);
    }
    if (out) {
        out->cbSize = sizeof *out;
        if (!GetMonitorInfoA(h, (MONITORINFO *)out)) {
            memset(out, 0, sizeof *out);
            out->cbSize = sizeof *out;
            out->rcMonitor.right = out->rcWork.right = GetSystemMetrics(SM_CXSCREEN);
            out->rcMonitor.bottom = out->rcWork.bottom = GetSystemMetrics(SM_CYSCREEN);
        }
    }
    return h;
}

static int MonIndexOf(HMONITOR h)  // the list entry of a monitor handle, -1 if none
{
    MONITORINFOEXA mi;
    int k;
    mi.cbSize = sizeof mi;
    if (!h || !GetMonitorInfoA(h, (MONITORINFO *)&mi)) return -1;
    for (k = 0; k < g_monN; ++k)
        if (!_stricmp(g_mons[k].dev, mi.szDevice)) return k;
    return -1;
}

// Monitor k's current mode in pixels (what the GOG wrapper reads as the desktop size); 0 when unknown
static int MonModeSize(int k, DWORD *w, DWORD *h)
{
    DEVMODEA dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    if (k < 0 || k >= g_monN || !EnumDisplaySettingsA(g_mons[k].dev, ENUM_CURRENT_SETTINGS, &dm) || !dm.dmPelsWidth || !dm.dmPelsHeight)
        return 0;
    *w = dm.dmPelsWidth;
    *h = dm.dmPelsHeight;
    return 1;
}

static const char *MonLabel(int k)  // v3.19.0: by rank: Main / Secondary / Tertiary / Quaternary Display, then "Display n"
{
    static const char *const kRank[4] = { "Main Display", "Secondary Display", "Tertiary Display", "Quaternary Display" };
    static char s[MON_MAX][32];
    if (k < 0 || k >= g_monN) return "the primary monitor";
    if (k < 4) return kRank[k];
    sprintf_s(s[k], sizeof s[k], "Display %d", k + 1);
    return s[k];
}

// Keeps the choice for the next start: the GDI name plus the two things that find the monitor again if Windows
// renumbers it.
static void MonSave(int k)
{
    if (g_iniFallback || k < 0 || k >= g_monN) return;
    WritePrivateProfileStringA("Window", "Monitor", g_mons[k].dev, g_iniPath);
    WritePrivateProfileStringA("Window", "MonitorName", g_mons[k].name, g_iniPath);
    WritePrivateProfileStringA("Window", "MonitorPath", g_mons[k].path, g_iniPath);
}

// At start (Init, after ReadIni): the monitor list and the remembered choice.
static void MonInit(void)
{
    const char *how = "the primary monitor (Monitor=0)";
    int k, sel = -1, digits = 1;
    const char *c;
    MONITORINFOEXA mi;
    g_monN = 0;
    EnumDisplayMonitors(NULL, NULL, MonEnumProc, 0);
    if (g_monN > 1) qsort(g_mons, g_monN, sizeof g_mons[0], MonCmp);
    for (k = 0; k < g_monN; ++k) {
        MonEntry *m = &g_mons[k];
        DISPLAY_DEVICEA dd;
        DWORD i;
        for (i = 0; i < 8; ++i) {  // the monitor(s) on this output: the first active one (else the first one)
            memset(&dd, 0, sizeof dd);
            dd.cb = sizeof dd;
            if (!EnumDisplayDevicesA(m->dev, i, &dd, EDD_GET_DEVICE_INTERFACE_NAME)) break;
            if (!m->path[0] || (dd.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
                strcpy_s(m->path, sizeof m->path, dd.DeviceID);
                strcpy_s(m->name, sizeof m->name, dd.DeviceString);
            }
            if (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) break;
        }
    }
    MonEdidNames();  // the EDID name replaces the device string ("Generic PnP Monitor") where Windows has one
    // the remembered monitor
    for (c = g_monIni; *c; ++c)
        if (*c < '0' || *c > '9') digits = 0;
    if (!g_monIni[0] || !strcmp(g_monIni, "0")) {
        sel = -1;
    } else if (digits) {
        for (k = 0; k < g_monN && sel < 0; ++k)
            if (g_mons[k].num == atoi(g_monIni)) sel = k, how = "by its number (Monitor=n)";
        if (sel < 0) how = "the primary monitor: no \\\\.\\DISPLAYn with that number";
    } else {
        for (k = 0; k < g_monN && sel < 0 && g_monPathIni[0]; ++k)
            if (!_stricmp(g_mons[k].path, g_monPathIni)) sel = k, how = "by its device path (MonitorPath)";
        if (sel < 0 && g_monNameIni[0]) {
            int n = 0, hit = -1;
            for (k = 0; k < g_monN; ++k)
                if (!_stricmp(g_mons[k].name, g_monNameIni)) ++n, hit = k;
            if (n == 1) sel = hit, how = "by its name (MonitorName)";
        }
        for (k = 0; k < g_monN && sel < 0; ++k)
            if (!_stricmp(g_mons[k].dev, g_monIni)) sel = k, how = "by its GDI name (Monitor)";
        if (sel < 0) how = "the primary monitor: the remembered one is not attached";
    }
    if (sel < 0) sel = MonPrimary();
    g_monSel = sel;
    for (k = 0; k < g_monN; ++k)
        Log("monitor %d: %s %s, %ldx%ld at %ld,%ld%s, \"%s\", %s", k + 1, MonLabel(k), g_mons[k].dev,
            g_mons[k].rc.right - g_mons[k].rc.left, g_mons[k].rc.bottom - g_mons[k].rc.top, g_mons[k].rc.left,
            g_mons[k].rc.top, g_mons[k].primary ? " (primary)" : "", g_mons[k].name, g_mons[k].path[0] ? g_mons[k].path : "(no device path)");
    Log("monitor: %s chosen, %s", g_monN ? MonLabel(sel) : "(none listed)", how);
    if (g_autoSize && g_monN) {  // Width / Height 0: three quarters of the chosen monitor's height, 4:3
        MonGet(sel, &mi);
        g_height = (mi.rcMonitor.bottom - mi.rcMonitor.top) * 3 / 4;
        g_width = g_height * 4 / 3;
        g_cliW = g_width;
        g_cliH = g_height;
    }
}


// ================================================================ v1.3: Aspect Ratio in Options

// Research: src\research\options_menu\report.md (menu), projection\report.md (3D aspect), gog_wrapper\report.md.
// Options > Option 1 gets an "Aspect Ratio" row under Resolution. Resolution keeps its six stock entries (the stock
// byte cfg+0x20 stays 0..5, so sh4.sav still suits the stock game) and shows the six sizes of the chosen shape; the
// shape lives in the Options block's unused padding byte cfg+0x21 (as shape + 1; 0 = not chosen yet), which the game
// copies, saves and loads with the block but never reads. Hooks (stock bytes checked first, all or nothing):
//   50C440  message lookup: bank 0 ids 7000h.. are this mod's strings (byte mode: 00 80, 100h - ASCII, FF FF)
//   523AA0  "row changed?" (eax row, edx page, ecx menu object): page 0 rows shift down by one under the new row
//   5242A0  Resolution setter: also adopts the shape (first start: entries 0-1 = 4:3, 2-5 = 16:9 = the stock sizes)
//   414917  size pick in 4148E0 (ecx = entry 0..5): writes [5D524C] / [5D5250] from the table, then the stock reset
//   41D600, 5A9360  camera aspect / zoom: GOG keyed them on the entry number (>= 2 = 16:9), now on the real size
//   407739, 405972  the game's GetCursorPos / SetCursorPos calls: window pixels <-> game pixels
//   data: page 0 row table (3 pointer copies + imm32 523E64), row counts 655034 / 655040, Exit row 655064, and the
//   Sound row number (5 -> 6) at 52572A, 52585B, 525298, 525CF3.
// v1.5 (Enable=1 and FitWrapper=1): a "Display Mode" row (Windowed / Borderless) above Resolution, kept in the next
// padding byte cfg+0x22 (mode + 1). Rows shift down by one more (Sound 7, Exit 8), the two "cursor on Resolution" tests
// for the wide-label arrow (5252B0 mouse, 52587C draw) follow Resolution to row 1 and also move the arrow for Display
// Mode, and page 0's rows are
// 28 apart instead of 32 (523DF4) so Exit stays above the description line. Borderless = the GOG wrapper's own
// borderless full screen: dxcfg display=desktop, aspect=enabled, scaling=fit (its back buffer is the monitor and it
// letterboxes the picture), applied by the same re-apply / device reset as a size change; the window becomes a popup
// over the whole monitor. Alt+Enter switches between the two.
#define CFG ((BYTE *)0x1091AD0)  // live Options block (the menu edits it in place); backup at 0x1091AF4
// v1.9: every deliberate size re-apply ([0x610DD8] = -1, so the main tick 4148E0 applies the size again with a device
// reset) bumps g_reapplyGen first, so PickSize can tell it from the stock retry of a failed reset (audit INT-1).
static volatile LONG g_reapplyGen;
static void ForceReapply(void)
{
    InterlockedIncrement(&g_reapplyGen);
    *(volatile int *)0x610DD8 = -1;
}
// v1.6: 21:9 and 32:9 are appended (stored shape byte cfg+0x21 = 4 / 5; it is shape + 1), so the stored 1..3 of 4:3,
// 16:10 and 16:9 keep their meaning; the row cycles in this order. v1.3 .. v1.5 read 4 / 5 as "from the entry" (4:3 or
// 16:9). (5:4 is left out: a shape narrower than 4:3 would need the 2D letterboxed vertically and Hor+ would show less
// than 4:3. 3:2 would need a display-order map over the stored ids to sit between 4:3 and 16:10.)
enum { ASP_43, ASP_1610, ASP_169, ASP_219, ASP_329, ASP_N };
static const char *const kAspName[ASP_N] = { "4:3", "16:10", "16:9", "21:9", "32:9" };
// Entry 0..5 per shape, 480p to 4K. Stock GOG: 640x480, 1024x768 (4:3) and 1366x768 .. 3840x2160 (16:9) keep
// their entry numbers. Labels longer than 8 characters get the right arrow moved out (ArrowShift; the stock game moved
// it for entries > 2, which every 4:3 / 16:10 / 16:9 label follows; 21:9's 2560x1080 is entry 2). The 21:9 monitor
// sizes are not one exact ratio (2.37 .. 2.4): the camera, the window and the 2D squeeze use the real size.
static const int kSizes[ASP_N][6][2] = {
    { { 640, 480 }, { 1024, 768 }, { 1280, 960 }, { 1600, 1200 }, { 2048, 1536 }, { 2880, 2160 } },
    { { 768, 480 }, { 1280, 800 }, { 1440, 900 }, { 1920, 1200 }, { 2560, 1600 }, { 3840, 2400 } },
    { { 854, 480 }, { 1280, 720 }, { 1366, 768 }, { 1920, 1080 }, { 2560, 1440 }, { 3840, 2160 } },
    { { 1720, 720 }, { 1920, 800 }, { 2560, 1080 }, { 3440, 1440 }, { 3840, 1600 }, { 5120, 2160 } },  // v1.6 21:9
    { { 1920, 540 }, { 2560, 720 }, { 3200, 900 }, { 3840, 1080 }, { 5120, 1440 }, { 7680, 2160 } },   // v1.6 32:9
};

enum { DM_WINDOWED, DM_BORDERLESS, DM_N };  // v1.5 Display Mode
static const char *const kDmName[DM_N] = { "Windowed", "Borderless" };

// v2.0 Field of View in degrees (user, 2026-11-03: "values from 60 to 120 in increments of 5"): the horizontal angle the
// usual third-person camera gets at the picture's shape (FOV_REF_V); every other camera is scaled by the same
// factor on tan(fov / 2), so per-room framing and zooms keep their relation. 0 = Default (the game's own).
#define FOV_N 14
static const int kFovDeg[FOV_N] = { 0, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120 };
static const char *const kFovName[FOV_N] = { "Default", "60", "65", "70", "75", "80", "85", "90", "95", "100", "105",
                                             "110", "115", "120" };
// v3.3.5: the FOV First Person row offers these entries of kFovDeg only (Default, 100..120); g_fovFpSel stays an index
// into kFovDeg, the row shows its position in this list (FovFpGet / FovFpInput)
#define FOVFP_N 6
static const int kFovFpIdx[FOVFP_N] = { 0, 9, 10, 11, 12, 13 };

// v1.9 Frame Rate (experimental, next start)
#define FPS_N 4
static const int kFps[FPS_N] = { 30, 60, 120, 180 };
static const char *const kFpsName[FPS_N] = { "30", "60", "120", "180" };
#define FPS_SHOWN (g_fpsHigh ? FPS_N : 2)  // v3.31 FPS331: the values the Options row offers


#define MSG_BASE 0x7000
enum { MSG_ASPECT = MSG_BASE, MSG_ASPECT_DESC, MSG_ASP0, MSG_RES0 = MSG_ASP0 + ASP_N, MSG_DISP = MSG_RES0 + 6, MSG_DISP_DESC,
       MSG_DM0, MSG_MON = MSG_DM0 + DM_N, MSG_MON_DESC, MSG_MON0, MSG_FOV = MSG_MON0 + MON_MAX, MSG_FOV_DESC, MSG_FOV0,
       MSG_FOVFP = MSG_FOV0 + FOV_N, MSG_FOVFP_DESC, MSG_FPS, MSG_FPS_DESC, MSG_FPS0, MSG_ITEM = MSG_FPS0 + FPS_N,
       MSG_ITEM_DESC, MSG_ITEM0, MSG_PG4 = MSG_ITEM0 + 2, MSG_PG5, MSG_PG6, MSG_EM0,
       MSG_EMD0 = MSG_EM0 + EM_N, MSG_SB = MSG_EMD0 + EM_N, MSG_SB_DESC, MSG_SBB,
       MSG_SBB_DESC, MSG_SBV0, MSG_PAD0 = MSG_SBV0 + SB_MAX + 1,
       MSG_GFX0 = MSG_PAD0 + 45, MSG_GFXD0 = MSG_GFX0 + 3, MSG_GFXV0 = MSG_GFXD0 + 3,  // v3.12.0; v3.39 PR339: MSG_GFX0 .. MSG_SUBT - 1 (24) = presentation.inc's texts (PRT_*), the same numbers
       MSG_SUBT = MSG_GFXV0 + 3 * 6, MSG_SUBL0, MSG_SUBD0 = MSG_SUBL0 + SUB_N + 1,  // v3.13.0: + Back
       MSG_SUBV0 = MSG_SUBD0 + SUB_N + 1, MSG_SUBROWD = MSG_SUBV0 + SUB_N * SUB_VMAX,
       MSG_VIBV0, MSG_VIBD = MSG_VIBV0 + 4,  // v3.14.0
       MSG_FXT, MSG_FXL0, MSG_FXD0 = MSG_FXL0 + FX_N + 1, MSG_FXV0 = MSG_FXD0 + FX_N + 1,  // v3.15.0: + Back
       MSG_FXROWD = MSG_FXV0 + FX_N * FX_VMAX,
       MSG_GP0, MSG_PC0 = MSG_GP0 + 128, MSG_HB0 = MSG_PC0 + 48, MSG_CAM0 = MSG_HB0 + 32, MSG_PERF0 = MSG_CAM0 + 256, MSG_INV0 = MSG_PERF0 + 8, MSG_SHD0 = MSG_INV0 + 16, MSG_DOF0 = MSG_SHD0 + 32, MSG_PP0 = MSG_DOF0 + 32, MSG_GT0 = MSG_PP0 + 48, MSG_FN0 = MSG_GT0 + 16, MSG_OM0 = MSG_FN0 + 16, MSG_RG0 = MSG_OM0 + 16, MSG_SB0 = MSG_RG0 + 16, MSG_RZ0 = MSG_SB0 + 16, MSG_RZ1 = MSG_RZ0 + 32, MSG_END = MSG_RZ1 + 32 };  // v3.49 MERGE: one MSG_RZ1 block for both (RZ349 rzmon.inc RMT_*: RZ0 + 28 .. 29 and RZ1 + 0 .. 4 of its RZ1 + 0 .. 15; RZ349L rzlooks.inc RZLM_*: RZ1 + 16 .. 23 of its RZ1 + 16 .. 31; RZ0 + 30 .. 31 unused)  // v3.49 RZ349: + the MSG_RZ1 block (w3 RZ349 rzmon.inc: RZ0 + 28 .. 29 and RZ1 + 0 .. 15; w4: RZ0 + 30 .. 31 and RZ1 + 16 .. 31)  // v3.49 RZ349L: + the MSG_RZ1 block (BRIEF_COMMON: w3 RZ1 + 0..15, w4 rzlooks.inc RZLM_* at RZ1 + 16.., 8 of 16)  // v3.48 RZ348: + the MSG_RZ0 block (randomizer.inc RZM_*, 25 of 32)  // v3.47 MERGE: the v3.47 text-id chain (BRIEF_COMMON): MSG_OM0 (w1 CT347 omni.inc OMM_*, 11 of 16), MSG_RG0 (w4 RG347 regen.inc RGT_*, 6 of 16), MSG_SB0 (w3 SB347: reserved, no text used)  // v3.46 OM346: + the MSG_OM0 block (omni.inc OMM_*, 11 of 16)  // v3.43 MERGE: the v3.43 text-id chain (BRIEF_COMMON; DOF = DF343 dof.inc DFM_*, PP = PP343 post.inc PPM_*, GT = GT343 teleport.inc, FN = FN343: reserved, font.inc has no row)  // v3.42 SM342: the SHD block 16 -> 32 (the Shadows page's Darkness row: SHM_N 11 -> 18)  // v3.40 SH340: MSG_SHD0.. = shadows.inc's texts (SHM_*; was MSG_END = MSG_INV0 + 16)  // v3.37 MERGE: MSG_INV0.. = inventory.inc's texts (INVM_*, W2 INV337)  // v3.37 CP337: MSG_PERF0.. = perfhud.inc's texts (PERFT_*; was MSG_END = MSG_CAM0 + 256)  // v3.33 CAM333: MSG_CAM0.. = camera.inc's texts (CAMT_*; was MSG_END = MSG_HB0 + 32)  // v3.29 merge: the gameplay block 64 -> 128 (v3.29 AFTERIMAGE / v3.30 ECHO / v3.31 ENEMIES each raised it to 96, SUPPLY DROPS to 80; GPM_N is 105 with all of them); PLAYER PACK: MSG_PC0.. = playercheats.inc's texts (PCM_*); HUD BARS: MSG_HB0.. = hudbars.inc's texts  // v3.25.0: MSG_GP0.. = gameplay.inc's texts (GPM_*)  // v3.10.0: MSG_PAD0..+44 = Button Config pad names (buttoncfg.inc)  // v3.30 MERGE: unchanged; GPM_N 121 of 128 (AS330 + 2, ECHO330 + 7, EN330 + 7)  // v3.31 MERGE: unchanged (no new text id in v3.31)  // v3.32 MERGE: GPM_N 124 of 128 (NC332 + 3), HBM_N 16 of 32 (DHS332 + 2)
static BYTE g_txtAspect[32], g_txtAspectDesc[48], g_txtAsp[ASP_N][16], g_txtRes[ASP_N][6][16];
static BYTE g_resLen[ASP_N][6];  // v1.6: characters of each Resolution label (the arrow rule)
static BYTE g_txtDisp[32], g_txtDispDesc[48], g_txtDm[DM_N][16];
static BYTE g_txtMon[32], g_txtMonDesc[48], g_txtMons[MON_MAX][32];  // v1.7; v3.19.0 32 (rank names)
static BYTE g_txtFov[32], g_txtFovDesc[48], g_txtFovs[FOV_N][16];  // v1.9
static BYTE g_txtFovFp[32], g_txtFovFpDesc[48];                     // v3.3: the first-person row (same values)
static BYTE g_txtFps[32], g_txtFpsDesc[48], g_txtFpss[FPS_N][16];  // v1.9
static BYTE g_txtItem[32], g_txtItemDesc[48], g_txtItems[2][16];   // v3.3: Item Sparkle (v3.3.5: Item Shine)
static BYTE g_txtSubT[32], g_txtSubL[SUB_N + 1][32], g_txtSubD[SUB_N + 1][56], g_txtSubV[SUB_N][SUB_VMAX][20], g_txtSubRowD[56];
static DWORD g_subIds[SUB_N][SUB_VMAX];
static BYTE g_txtVibV[4][16], g_txtVibD[56];  // v3.14.0
static DWORD g_vibIds[4];
static BYTE g_txtFxT[32], g_txtFxL[FX_N + 1][32], g_txtFxD[FX_N + 1][56], g_txtFxV[FX_N][FX_VMAX][24], g_txtFxRowD[56];  // v3.15.0
static DWORD g_fxIds[FX_N][FX_VMAX];
static BYTE g_txtPg[3][32];                                         // v3.5.0: titles of Options 4, 5, 6
static BYTE g_txtEm[EM_N][32], g_txtEmD[EM_N][56];                  // v3.7.0: Option 6 labels / descriptions
static BYTE g_txtSb[2][32], g_txtSbD[2][56], g_txtSbv[SB_MAX + 1][8];  // v3.8.0: Save Banks rows, bank numbers, "-"
static DWORD g_sbIds[SB_MAX + 1];  // [SB_MAX] = "-" (banks Off; review SB-002)
static DWORD g_aspIds[ASP_N], g_resIds[6], g_dmIds[DM_N], g_monIds[MON_MAX], g_fovIds[FOV_N], g_fpsIds[FPS_N];
static DWORD g_fovFpIds[FOVFP_N];          // v3.3.5: the first-person row's labels (kFovFpIdx entries of kFovName)
static DWORD g_itemIds[2];                 // v3.3
static int g_fovRow;                       // v1.9: the Field of View rows are there; v3.3: 2 rows (1st / 3rd Person FOV)
static int g_fpsRow;                       // v1.9: the Frame Rate row is there
// v3.5.0: Options 4 (Presentation) / 5 (Gameplay) / 6 (Extra Menus) row tables, writable (the game keeps a per-row
// value at +8). Option 4: Display Mode, Monitor, Aspect Ratio, 1st / 3rd Person FOV, Frame Rate, Exit.
static DWORD g_rowsSub[(SUB_N + 1) * 7 + 1], g_rowsOthers[8 * 7 + 1];  // v3.13.0
static DWORD g_rowsControl[7 * 7 + 1];  // v3.14.0
static DWORD g_rowsFx[(FX_N + FX_PP_ROWS + FX_DOF_ROWS + 1) * 7 + 1];  // v3.15.0; v3.43 PP343: + FX_PP_ROWS; v3.44 DF344: + FX_DOF_ROWS
static DWORD g_rowsPlayer[11 * 7 + 1], g_rowsEnemy[7 * 7 + 1];  // v3.37 MN337: Player 10 -> 11 (One-Hit Kill joined, Curse Meter left: 10 with PE)  // v3.25.0 (gameplay.inc); PLAYER PACK: 9 (8 rows + Back; was 5); HUD BARS: + Curse Meter (v3.29 merge: 10, the page limit)  // v3.31 ENEMIES: 4 rows (was 2); HUD BARS: + Health Bars; SUPPLY DROPS: + Supply Drops (v3.29 merge: 6)  // v3.30 EN330: + Revive Enemies (7)
static DWORD g_rowsSpecial[6 * 7 + 1];  // v3.27.0: the Special page (room for the later special-mod rows)
static DWORD g_rowsRando[11 * 7 + 1];  // v3.48 RZ348: the Randomizer page (6 rows today; room for Monsters, Mix, Ghosts, Looks)
static DWORD g_rowsPres[11 * 7 + 1], g_rowsPlay[10 * 7 + 1], g_rowsExtra[(EM_N + 3) * 7 + 1];  // v3.37 MN337: Option 5 5 -> 9 rows  // v3.48 RZ348: 9 -> 10 rows (+ Randomizer)
static int g_playShineRow;  // v3.37 MN337: Item Shine's row on Option 5 (gameplay.inc GpBuildPages; 0 without the folders)
static DWORD g_rowShine[7];  // v3.37 MN337: Item Shine's row, laid out with the others by GpBuildPages
static int g_presDisp = -1, g_presMon = -1;  // v3.5.0: Option 4 rows of Display Mode / Monitor (-1 = not there)
static int g_presAsp = -1;                    // v3.5.0: Option 4 row of Aspect Ratio
static int g_presFovFp = -1, g_presFov = -1, g_presFps = -1;  // v3.5.1: Option 4 rows of the ini-kept settings
// v3.5.1 (user: "make all our options change to yellow when altered and give the exit prompt when leaving like the
// vanilla options do"): the ini-kept settings as they were when Options opened (ResetPages). They still apply at once;
// a row differing from its snapshot is drawn yellow, makes the exit ask, and "No" puts the snapshot back (DiscardOwn).
static LONG g_snapFov, g_snapFovFp, g_snapFps, g_snapShine;
static LONG g_snapEm[EM_N];  // v3.7.0: Option 6 rows as Options opened
static LONG g_snapSbOn, g_snapSbBank;  // v3.8.0
static int g_snapOn;
static int g_slotPage[3] = { 0, 1, 2 };       // v3.5.0: the logical page each physical slot holds (LoadSlot)
static void SparkleUserChanged(int on);    // items.inc
static volatile LONG g_fpsSel;             // v1.9: index into kFps chosen in the menu (for the next start)
static volatile LONG g_fovSel;             // v2.0: index into kFovDeg (0 = Default); v3.3: third person only
static volatile LONG g_fovFpSel;           // v3.3: the same for Room 302's first-person camera
static DWORD g_fovRawBits, g_fovOutBits;   // v2.0: the last angle a camera asked for, and what was stored for it
static int g_fovLastFp;                    // v3.3: that camera was Room 302's first-person one
static int g_dispRow;                      // v1.5: the Display Mode row is there (Enable=1, FitWrapper=1)
static int g_monRow;                       // v1.7: the Monitor row is there (row 1: Display Mode row and 2+ monitors)
static int g_dmShift, g_monShift;          // v1.7: the right arrow's shift on the Display Mode / Monitor row (fixed per row)
// v3.19.0 C2b (staging/menus_v319/PLAN.md): values over 14 characters move their column 64 px left (the game's Dolby
// Digital mechanism) and put the right arrow 6 px after the text; widths at 10.4 canvas px a character (RE 4)
#define LONGVAL_CHARS 14
#define LONGVAL_MOVE 64
static int g_monLongW;                     // the Monitor row's longest rank name width when it is long (0 = not)
static int g_drawSlot = -1, g_drawRow = -1;  // the row 0x525990 draws next (RowChangedDraw), for the 0x52570E stub
static int g_mouseAdj;                     // right-arrow mouse rect adjustment of the 0x525CE3 stub (shift - move)
static int TextW(int chars) { return (int)(chars * 10.4f + 0.5f); }
static int g_monApplied = -1, g_monBooted; // v1.7: the monitor the setter last applied; the start-up one came from the ini
static volatile LONG g_dm = DM_WINDOWED;   // v1.5: the mode the wrapper is told (dxcfg) and the window takes
static int g_dmApplied = -1, g_dmBooted;  // the mode the setter last applied; the start-up mode was taken from the ini
static int g_applied = -1, g_bootDone;  // the shape the game size was last picked with (game thread)
static volatile LONG g_picW, g_picH;    // the game size last picked: the window's shape
static HANDLE g_reshapeEvt;             // set when the window should take the picture's shape (Watch thread)

static void Encode(BYTE *out, size_t n, const char *s)  // byte-mode game string
{
    size_t k = 0;
    out[k++] = 0x00;
    out[k++] = 0x80;
    for (; *s && k + 2 < n; ++s) out[k++] = *s == ' ' ? 0 : (BYTE)(0x100 - (BYTE)*s);
    out[k++] = 0xFF;
    out[k] = 0xFF;
}

static int LiveDisp(void)
{
    BYTE m = CFG[0x22];
    return m >= 1 && m <= DM_N ? m - 1 : DM_WINDOWED;
}

static int LiveMon(void)  // v1.7: cfg+0x23 = monitor + 1 (the last padding byte of the Options block)
{
    BYTE m = CFG[0x23];
    return m >= 1 && m <= g_monN ? m - 1 : (int)g_monSel;
}

static int LiveAspect(void)
{
    BYTE a = CFG[0x21];
    return a >= 1 && a <= ASP_N ? a - 1 : (CFG[0x20] >= 2 ? ASP_169 : ASP_43);
}

// The Resolution labels follow the live byte, so they preview the new list while the player scrolls Aspect Ratio.
static const BYTE *PadText(int i);  // v3.10.0, buttoncfg.inc
static const BYTE *GpText(int id);  // v3.25.0, gameplay.inc
static const BYTE *HbText(int id);  // HUD BARS, hudbars.inc
// v3.33 CAM333: camera.inc (included before gameplay.inc): Option 6's Camera row and the Camera page (LP_CAM)
static const BYTE *CamText(int id);
static void CamSnapshot(void);
static int CamChanged(void);
static int CamRowChanged(int row);
static void CamDiscard(void);
static int CamRowGrey(int slot, int row);
static int NcValueIdGreyed(int id);  // v3.33 MERGE: noclip.inc (Fly greyed while no over-the-shoulder camera is on)
static const BYTE *InvText(int id);  // v3.37 INV337: inventory.inc (Extended Toolbelt / Stackable Items rows)
static int HbValueIdGreyed(int id);  // v3.37 MN337: hudbars.inc (Curse Meter's Player / Both greyed without Playable Eileen)
static int CamLongValW(int page, int row);
static int CamArrowShift(int page, int cur);
// v3.37 CP337: perfhud.inc (included last): the backend line, the overlay's two calls (v3.39 PR339: no Options row)
static const BYTE *PerfText(int id);
static int PerfDescId(int stockId, int which);
static void PerfInstall(void);
static void PerfFrame(int where);
// v3.39 PR339: presentation.inc (before perfhud.inc): Option 4's Renderer and Opening rows
static const BYTE *PresText(int id);
static int PresFillRows(DWORD *t, int n);
static void PresSnapshot(void);
static int PresChanged(void);
static int PresRowChanged(int row);
static void PresApply(void);
static void PresDiscard(void);
static int PresRowGrey(int slot, int row);
static int PresValueIdGreyed(int id);
static const char *FxFilterDesc(void);  // v3.39 PR339: screenfx.inc (the Filter row's description names PreviewKey)
// v3.43 PP343: post.inc (at the end of screenfx.inc): the Screen Effects page's Bloom / Colour Grading / Otherworld Lens rows
static const BYTE *PpText(int id);
static void PpSnapshot(void);
static int PpChanged(void);
static int PpRowChanged(int row);
static void PpDiscard(void);
static int PpBuildRows(DWORD *rows);
// v3.40 SH340: shadows.inc (included after screenfx.inc): Shadow Edge, the Shadows page (LP_SHD) and its preview
static const BYTE *ShText(int id);
static const BYTE *GtText(int id);  // v3.43 GT343: teleport.inc (Ghost Teleport's row, the MSG_GT0 block)
static const BYTE *FnText(int id);  // v3.44 FN344: font.inc (Option 1's Menu Font row, the MSG_FN0 block)
static const BYTE *OmText(int id);  // v3.46 OM346: omni.inc (Stick Movement's row, the MSG_OM0 block)  // v3.48 OM348: Omni-Directional Movement
static int OmChanged(void);                  // v3.47 CT347: omni.inc (the row on Option 2: PadChanged asks it)
static int OmPageRowChanged(int page, int row);  // v3.47 CT347: -1 = not the Stick Movement row
static void OmRowInstall(void);              // v3.47 CT347: Option 2 gets the Stick Movement row
static const BYTE *RgText(int id);  // v3.47 RG347: regen.inc (the Regeneration row's lines, the MSG_RG0 block)
static const BYTE *RzText(int id);  // v3.48 RZ348: randomizer.inc (the Randomizer page's texts, the MSG_RZ0 block)
static const BYTE *RzlText(int id);  // v3.49 RZ349L: rzlooks.inc (Eileen Outfit / Ghosts rows, MSG_RZ1 + 16 ..)
static int RgRowGrey(int slot, int row);  // v3.47 RG347: the Regeneration row grey while Playable Eileen's own regeneration heals
static int RgValueIdGreyed(int id);  // v3.47 RG347: its value (Off) grey too
static int FnChanged(void);         // v3.44 FN344
static int FnRowChanged(int row);   // v3.44 FN344: -1 = not the Menu Font row
static void FnSnapshot(void);       // v3.44 FN344
static void FnDiscard(void);        // v3.44 FN344
static void FnRowInstall(void);     // v3.44 FN344
static void ShSnapshot(void);
static int ShChanged(void);
static int ShRowChanged(int row);
static void ShDiscard(void);
static int ShRowWanted(void);
static void ShRowInstall(DWORD *row);
static int ShMenuOn(void);
static void ShMenuOff(const char *why);
static void ShBuildPage(const DWORD *stockRows);
static int ShPageShown(void);
static int ShBackdrop(void *dev);
static void ShPreviewDraw(void);
static void ShFrameEnd(void);
static void ShadowsInstall(void);
static int ShPauseGrab(void *dev, LONG seq);                       // v3.41 SH341: screenfx.inc's pause picture on the GPU
static void *ShPauseTex(void *dev, LONG seq, UINT *w, UINT *h);    // v3.41 SH341
// v3.43 DF343: dof.inc (included after shadows.inc): Depth of Field, its page (LP_DOF) and its preview
static const BYTE *DofText(int id);
static void DofSnapshot(void);
static int DofChanged(void);
static int DofRowChanged(int row);
static void DofDiscard(void);
static int DofRowWanted(void);
static void DofRowInstall(DWORD *row);
static int DofMenuOn(void);
static void DofMenuOff(const char *why);
static void DofBuildPage(const DWORD *stockRows);
static void DofInstall(void);
static int DofBuildRows(DWORD *rows, int row0);  // v3.44 DF344: its rows on the Screen Effects page
static int DofFxRowChanged(int row);             // v3.44 DF344: -1 = not one of its rows
static const BYTE *__cdecl MsgText(int id)
{
    if (id == MSG_ASPECT) return g_txtAspect;
    if (id == MSG_ASPECT_DESC) return g_txtAspectDesc;
    if (id >= MSG_ASP0 && id < MSG_ASP0 + ASP_N) return g_txtAsp[id - MSG_ASP0];
    if (id == MSG_DISP) return g_txtDisp;
    if (id == MSG_DISP_DESC) return g_txtDispDesc;
    if (id >= MSG_DM0 && id < MSG_DM0 + DM_N) return g_txtDm[id - MSG_DM0];
    if (id == MSG_MON) return g_txtMon;
    if (id == MSG_MON_DESC) return g_txtMonDesc;
    if (id >= MSG_MON0 && id < MSG_MON0 + MON_MAX) return g_txtMons[id - MSG_MON0];
    if (id == MSG_FOV) return g_txtFov;
    if (id == MSG_FOV_DESC) return g_txtFovDesc;
    if (id >= MSG_FOV0 && id < MSG_FOV0 + FOV_N) return g_txtFovs[id - MSG_FOV0];
    if (id == MSG_FOVFP) return g_txtFovFp;
    if (id == MSG_FOVFP_DESC) return g_txtFovFpDesc;
    if (id == MSG_FPS) return g_txtFps;
    if (id == MSG_FPS_DESC) return g_txtFpsDesc;
    if (id >= MSG_FPS0 && id < MSG_FPS0 + FPS_N) return g_txtFpss[id - MSG_FPS0];
    if (id == MSG_ITEM) return g_txtItem;
    if (id == MSG_ITEM_DESC) return g_txtItemDesc;
    if (id >= MSG_ITEM0 && id < MSG_ITEM0 + 2) return g_txtItems[id - MSG_ITEM0];
    if (id >= MSG_PG4 && id <= MSG_PG6) return g_txtPg[id - MSG_PG4];
    if (id >= MSG_EM0 && id < MSG_EM0 + EM_N) return g_txtEm[id - MSG_EM0];   // v3.7.0
    if (id >= MSG_EMD0 && id < MSG_EMD0 + EM_N) return g_txtEmD[id - MSG_EMD0];
    if (id == MSG_SB || id == MSG_SBB) return g_txtSb[id == MSG_SBB];  // v3.8.0
    if (id == MSG_SB_DESC || id == MSG_SBB_DESC) return g_txtSbD[id == MSG_SBB_DESC];
    if (id >= MSG_SBV0 && id <= MSG_SBV0 + SB_MAX) return g_txtSbv[id - MSG_SBV0];
    if (id >= MSG_PAD0 && id < MSG_PAD0 + 45) return PadText(id - MSG_PAD0);  // v3.10.0
    if (id == MSG_SUBT) return g_txtSubT;  // v3.13.0
    if (id >= MSG_SUBL0 && id <= MSG_SUBL0 + SUB_N) return g_txtSubL[id - MSG_SUBL0];
    if (id >= MSG_SUBD0 && id <= MSG_SUBD0 + SUB_N) return g_txtSubD[id - MSG_SUBD0];
    if (id >= MSG_SUBV0 && id < MSG_SUBV0 + SUB_N * SUB_VMAX) return g_txtSubV[(id - MSG_SUBV0) / SUB_VMAX][(id - MSG_SUBV0) % SUB_VMAX];
    if (id == MSG_SUBROWD) return g_txtSubRowD;
    if (id >= MSG_VIBV0 && id < MSG_VIBV0 + 4) return g_txtVibV[id - MSG_VIBV0];  // v3.14.0
    if (id == MSG_VIBD) return g_txtVibD;
    if (id == MSG_FXT) return g_txtFxT;  // v3.15.0
    if (id >= MSG_FXL0 && id <= MSG_FXL0 + FX_N) return g_txtFxL[id - MSG_FXL0];
    if (id >= MSG_FXD0 && id <= MSG_FXD0 + FX_N) return g_txtFxD[id - MSG_FXD0];
    if (id >= MSG_FXV0 && id < MSG_FXV0 + FX_N * FX_VMAX) return g_txtFxV[(id - MSG_FXV0) / FX_VMAX][(id - MSG_FXV0) % FX_VMAX];
    if (id == MSG_FXROWD) return g_txtFxRowD;
    if (id >= MSG_RZ1 + 16 && id < MSG_END) return RzlText(id);  // v3.49 RZ349L (before RzText: its check runs to MSG_END)
    if (id >= MSG_RZ0 && id < MSG_RZ1 + 16) return RzText(id);  // v3.48 RZ348 (the newest block first)  // v3.49 MERGE: ends at RZ349L's MSG_RZ1 + 16 (was MSG_END; RzText sends RZ0 + 28 .. 29 and RZ1 + 0 .. 15 to RzMon's RmText)
    if (id >= MSG_RG0 && id < MSG_SB0) return RgText(id);  // v3.47 RG347 (the newest block first)  // v3.47 MERGE: OmText now ends at MSG_RG0; no MSG_SB0 line (w3 SB347 uses no text id)
    if (id >= MSG_OM0 && id < MSG_RG0) return OmText(id);  // v3.46 OM346 (first: the checks below run to MSG_END)  // v3.47 CT347: ends at MSG_RG0
    if (id >= MSG_FN0 && id < MSG_OM0) return FnText(id);  // v3.44 FN344 (v3.46 OM346: ends at MSG_OM0, was MSG_END)
    if (id >= MSG_GT0 && id < MSG_FN0) return GtText(id);  // v3.43 GT343 (first: the SHD check below runs to MSG_END)
    if (id >= MSG_PP0 && id < MSG_GT0) return PpText(id);  // v3.43 PP343 (before ShText: its check runs to MSG_END)
    if (id >= MSG_DOF0 && id < MSG_PP0) return DofText(id);  // v3.43 DF343 (before the Shadows check: it runs to MSG_END)
    if (id >= MSG_SHD0 && id < MSG_END) return ShText(id);  // v3.40 SH340 (first: the checks below run to MSG_END)
    if (id >= MSG_INV0 && id < MSG_END) return InvText(id);  // v3.37 INV337 (first: the checks below run to MSG_END)
    if (id >= MSG_PERF0 && id < MSG_END) return PerfText(id);  // v3.37 CP337 (before the camera's: its check runs to MSG_END)
    if (id >= MSG_CAM0 && id < MSG_END) return CamText(id);  // v3.33 CAM333 (first: the HB / GP checks below run to MSG_END)
    if (id >= MSG_HB0 && id < MSG_END) return HbText(id);  // HUD BARS (before GpText: its range check ends at MSG_END)
    if (id >= MSG_GP0 && id < MSG_END) return GpText(id);  // v3.25.0
    if (id >= MSG_GFX0 && id < MSG_SUBT) return PresText(id);  // v3.39 PR339: Renderer / Opening (was V-Sync / AA / AF, v3.12.0)
    return g_txtRes[LiveAspect()][id - MSG_RES0];
}

static const DWORD g_backMsg = 0x50C447, g_backChanged = 0x523AA6, g_backSize = 0x4149A9;

static __declspec(naked) void hkMsgLookup(void)  // const BYTE *__cdecl (int bank, int id)
{
    __asm {
        mov eax, [esp + 4]
        test eax, eax
        jnz stock
        mov eax, [esp + 8]
        cmp eax, MSG_BASE
        jb stock
        cmp eax, MSG_END
        jae stock
        push eax
        call MsgText
        add esp, 4
        ret
    stock:
        push esi                 // the 7 stolen bytes, then the stock jge at 50C447
        mov esi, [esp + 8]
        test esi, esi
        jmp dword ptr [g_backMsg]
    }
}

// v3.5.0: Option 1 is the stock page again. Aspect Ratio, Display Mode and Monitor (Option 4) still live in the
// Options block's padding (cfg+0x21 / +0x22 / +0x23), which no stock row covers: the Resolution row (page 0, row 0)
// reports them as changed too, so an edit to only one of them still asks "Do you want to change the options?" and
// keep applies them (SetResIndex runs on every keep). The stock row-0 test still runs first: it sets the Resolution
// setter's gate [0x1092220] from the Resolution byte alone, as stock. Other rows / pages: stock. Rows of Options 4-6
// are never asked (the change scan 523D20 walks pages 0-2 with the stock counts 0x655034).
static int EmChanged(void)  // v3.7.0: any Option 6 row differs from its value when Options opened
{
    int k;
    if (!g_snapOn) return 0;
    for (k = 0; k < EM_N; ++k)
        if (g_emOn[k] != g_snapEm[k]) return 1;
    return g_sbOn != g_snapSbOn || g_sbBank != g_snapSbBank;  // v3.8.0
}
static int SubChanged(void);  // v3.13.0
static int FxChanged(void);   // v3.15.0
static int GpChanged(void);   // v3.25.0 (gameplay.inc)
static int GpRowChanged(int page, int row);
static void GpSnapshot(void);
static void GpDiscard(void);
static int GpSubmenuRow(int page, int row);
static int g_lpParent[LP_MAX] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };  // v3.48 RZ348: 15 entries  // v3.43 DF343: 13 entries  // v3.25.0: hidden page -> its parent page
static int g_lpParentRow[LP_MAX] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };  // v3.48 RZ348: 15 entries  // v3.43 DF343: 13 entries  // and the row that opens it
static int __cdecl PadChanged(const BYTE *obj)  // v3.5.1: also the ini-kept settings against their snapshot
{
    return obj[0xB0 + 0x21] != obj[0xD4 + 0x21] || obj[0xB0 + 0x22] != obj[0xD4 + 0x22] ||
           obj[0xB0 + 0x23] != obj[0xD4 + 0x23] ||
           (g_snapOn && (g_fovSel != g_snapFov || g_fovFpSel != g_snapFovFp || g_fpsSel != g_snapFps ||
                         (g_sparkleOn != 0) != (g_snapShine != 0) || EmChanged() || PresChanged() || SubChanged() || VibChanged() ||
                         FxChanged() || GpChanged() || CamChanged() || ShChanged() || PpChanged() || DofChanged() ||
                         FnChanged() || OmChanged()));  // v3.47 CT347: + OmChanged (Option 2's Stick Movement; was in GpChanged)  // v3.44 FN344: + FnChanged  // v3.43 MERGE: + PpChanged (PP343), + DofChanged (DF343)  // v3.40 SH340: + ShChanged  // v3.33 CAM333: + CamChanged; v3.39 PR339: PresChanged (was GfxChanged, PerfChanged)
}

static __declspec(naked) void StockRowChanged(void)  // the stock 523AA0 as a subroutine (eax row, edx page, ecx obj)
{
    __asm {
        push ebx                 // the 6 stolen bytes
        push esi
        mov esi, eax
        xor eax, eax
        jmp dword ptr [g_backChanged]
    }
}

// v3.5.0 (in-game test T8): 523AA0 has two callers. The change scan 523D20 (returns to 523D49) asks about stock pages
// 0-2 by number; the row draw 525990 (returns to 525A18) asks about the physical slot it draws, to colour changed rows
// yellow. A slot holding Option 4 / 5 must answer for its own rows, not for the stock page of that slot number (the
// first build coloured Option 4's Display Mode by Option 1's Resolution test). -1 = let the stock test answer.
static void SubmenuArrow(int row, int slot, const BYTE *obj, int xoff);
static int __cdecl RowChangedDraw(int row, int slot, const BYTE *obj, int xoff)
{
    int k = 0;
    g_drawSlot = slot;  // v3.19.0 C2b: the 0x52570E stub asks about this row
    g_drawRow = row;
    SubmenuArrow(row, slot, obj, xoff);  // v3.19.0 C3; v3.21.0: + the slot's slide offset
    if ((unsigned)slot < 3 && g_slotPage[slot] == 1 && row == 2 && VibChanged()) return 1;  // v3.14.0: the level
    if ((unsigned)slot < 3 && g_slotPage[slot] == 1) {  // v3.47 CT347: Option 2's Stick Movement row
        const int o = OmPageRowChanged(1, row);
        if (o >= 0) return o;
    }
    if ((unsigned)slot < 3 && g_slotPage[slot] == 0) {  // v3.44 FN344: Option 1's Menu Font row
        const int f = FnRowChanged(row);
        if (f >= 0) return f;
    }
    if ((unsigned)slot >= 3 || g_slotPage[slot] < 3) return -1;  // a stock page in its own slot: stock (gate included)
    if (g_slotPage[slot] == LP_SUB) {  // v3.13.0: the Subtitle Style page; its row 0 draws the preview once a frame
        if (row == 0 && slot == *(volatile int *)0x1091B1C) SubPreview();
        return g_snapOn && row >= 0 && row < SUB_N && g_sub[row] != g_snapSub[row];
    }
    if (g_slotPage[slot] == LP_SHD) return ShRowChanged(row);  // v3.40 SH340: the Shadows page
    if (g_slotPage[slot] == LP_DOF) return DofRowChanged(row);  // v3.43 DF343: the Depth of Field page
    if (g_slotPage[slot] == LP_FX) {  // v3.15.0: the Screen Effects page; its row 0 marks the preview frame (FX-002)
        if (row == 0 && slot == *(volatile int *)0x1091B1C) InterlockedExchange(&g_fxStamp, g_fxFrame);
        {   // v3.44 DF344: Depth of Field / DoF Strength (after the post rows)
            const int d = DofFxRowChanged(row);
            if (d >= 0) return d;
        }
        if (row >= FX_N) return PpRowChanged(row - FX_N);  // v3.43 PP343 (Back: not one of them)
        return g_snapOn && row >= 0 && row < FX_N && g_fx[row] != g_snapFx[row];
    }
    {   // v3.25.0: Option 5's Player / Enemies pages
        const int c = GpRowChanged(g_slotPage[slot], row);
        if (c >= 0) return c;
    }
    if (g_slotPage[slot] == 4) return g_snapOn && row == g_playShineRow && (g_sparkleOn != 0) != (g_snapShine != 0);  // Item Shine (v3.37 MN337: its recorded row, was 0)
    if (g_slotPage[slot] == 5) {  // v3.7.0; v3.8.0 rows EM_N / EM_N + 1 = Save Banks / Save Bank
        if (!g_snapOn || row < 0) return 0;
        if (row < EM_N) return g_emOn[row] != g_snapEm[row];
        if (row == EM_N) return g_sbOn != g_snapSbOn;
        if (row == EM_N + 1) return CamChanged();  // v3.33 CAM333: the Camera row (was Save Bank)
        return 0;
    }
    if (g_slotPage[slot] != 3) return 0;
    if (row == g_presAsp) k = 0x21;
    else if (row == g_presDisp) k = 0x22;
    else if (row == g_presMon) k = 0x23;
    if (k) return obj[0xB0 + k] != obj[0xD4 + k];
    if (!g_snapOn) return 0;
    if (row == g_presFovFp) return g_fovFpSel != g_snapFovFp;
    if (row == g_presFov) return g_fovSel != g_snapFov;
    if (row == g_presFps) return g_fpsSel != g_snapFps;
    k = PresRowChanged(row);  // v3.39 PR339: Renderer / Opening (were Performance, V-Sync, AA, AF)
    return k > 0;
}

static __declspec(naked) void hkRowChanged(void)  // 523AA0: eax row, edx page (scan) or slot (draw), ecx menu object
{
    __asm {
        cmp dword ptr [esp], 525A18h  // the row draw?
        jne scan
        push eax                 // the row, for the stock test
        push ecx
        push edx
        push dword ptr [esp + 54h]  // v3.21.0: 525990's arg5 (the slide offset; [esp+48h] at entry + 12)
        push ecx
        push edx
        push eax
        call RowChangedDraw
        add esp, 16
        pop edx
        pop ecx
        cmp eax, -1
        je drawstock
        add esp, 4
        ret
    drawstock:
        pop eax
        jmp stock
    scan:
        test edx, edx
        jnz stock
        test eax, eax
        jnz stock
        push ecx                 // callers (the change scan 523D20) keep ecx / edx across calls
        push edx
        call StockRowChanged     // Resolution (sets the gate)
        pop edx
        pop ecx
        test eax, eax
        jnz done
        push ecx
        push edx
        push ecx
        call PadChanged
        add esp, 4
        pop edx
        pop ecx
    done:
        ret
    stock:
        push ebx
        push esi
        mov esi, eax
        xor eax, eax
        jmp dword ptr [g_backChanged]
    }
}

typedef int(__cdecl *Fn0)(void);
typedef int(__cdecl *Fn1)(int);
typedef int(__cdecl *Fn2)(int, int);

static int __cdecl AspectGet(void) { return LiveAspect(); }
static int __cdecl AspectDesc(void) { return g_dispRow ? MSG_ASPECT_DESC : PerfDescId(MSG_ASPECT_DESC, 1); }  // v3.37 CP337: + the backend line when it is the first row
static int __cdecl DispGet(void) { return LiveDisp(); }
static int __cdecl DispDesc(void) { return PerfDescId(MSG_DISP_DESC, 0); }  // v3.37 CP337: + the backend line (perfhud.inc)
static int __cdecl MonRowGet(void) { return LiveMon(); }
static int __cdecl MonDesc(void) { return MSG_MON_DESC; }

// Row input, the same tests as the Sound row (524B60): confirm / arrow click / pad left (4) or right (8). Returns the
// next value 0..count-1, or -1 when nothing was pressed.
static int StepValue(int v, int count)
{
    int pad = ((Fn1)0x5542C0)(0), left;
    if (!((Fn2)0x4F84A0)(0, 0x11) && !(*(int *)0x1092214 && ((Fn0)0x405490)()) &&
        !(*(int *)0x1092218 && ((Fn0)0x405490)()) && !(pad & 0x0C))
        return -1;
    left = (pad & 4) || (*(int *)0x1092214 && ((Fn0)0x405490)());
    ((Fn1)0x56CC40)(0x9C43);  // the stock "value changed" sound
    return left ? (v + count - 1) % count : (v + 1) % count;
}

static int __cdecl AspectInput(BYTE *cfg, int count)
{
    const int v = StepValue(AspectGet(), count);
    if (v >= 0) cfg[0x21] = (BYTE)(v + 1);
    return 0;
}

static int __cdecl DispInput(BYTE *cfg, int count)
{
    const int v = StepValue(LiveDisp(), count);
    if (v >= 0) cfg[0x22] = (BYTE)(v + 1);
    return 0;
}

static int __cdecl MonInput(BYTE *cfg, int count)  // v1.7
{
    const int v = StepValue(LiveMon(), count);
    if (v >= 0) cfg[0x23] = (BYTE)(v + 1);
    return 0;
}

// ---------------------------------------------------------------- v1.9: field of view
// The gameplay cameras set the field of view (radians, the full vertical angle) through 0x4F70A0, which only stores
// its argument in [0xFD5B80]: the world rig 0x506270 every frame at 0x50629D (each room's camera record, 63 deg in most,
// times the run zoom; a scripted shot's own value), and Room 302's first-person camera at 0x500829 / 0x500B9E. The
// frame's projection reads [0xFD5B80] at 0x4EF7D9 (call 0x55BE50); cutscenes write it by their own paths and keep their
// framing. The setter is replaced by FovSet: fov' = 2 atan(k tan(fov / 2)), so the picture widens evenly and the
// frustum culling (built from the same angle) follows; Hor+ (the camera zoom, CamZoomBits) still widens it sideways.
// Playable Eileen's over-the-shoulder Fov (its call at 0x50629D) goes through 0x4F70A0 too, so both apply.
// v2.0: the value is degrees (0 = Default); the nearest row entry
static int FovIndexOf(int deg)
{
    int i, best = 0;
    if (deg <= 0) return 0;
    for (i = 1; i < FOV_N; ++i)
        if (best == 0 || abs(kFovDeg[i] - deg) < abs(kFovDeg[best] - deg)) best = i;
    return best;
}

// The factor on tan(fov / 2): the chosen horizontal angle over the reference camera's horizontal angle at the
// picture's shape (Hor+: the vertical view is the 4:3 one, the sides widen with the shape).
// v3.3: fp = Room 302's first-person camera, measured against its own 55 deg (FOV_REF_FP_V), so its row's degrees are
// its real side-to-side angle too.
static float FovFactor(int fp)
{
    const int deg = kFovDeg[fp ? g_fovFpSel : g_fovSel];
    const int w = *(int *)0x5D524C, h = *(int *)0x5D5250;
    float aspect = w > 0 && h > 0 ? (float)w / (float)h : 4.0f / 3.0f;
    if (deg <= 0) return 1.0f;
    if (!(aspect > 0.5f && aspect < 8.0f)) aspect = 4.0f / 3.0f;
    return tanf((float)deg * 0.00872664626f) / (tanf(0.5f * (fp ? FOV_REF_FP_V : FOV_REF_V)) * aspect);
}

static DWORD __cdecl FovBits(DWORD bits, int fp)
{
    float f, t, k;
    if ((fp ? g_fovFpSel : g_fovSel) == 0) return bits;
    memcpy(&f, &bits, 4);
    if (!(f > 0.01f && f < 3.0f)) return bits;  // not an angle we know: leave it
    k = FovFactor(fp);
    t = tanf(0.5f * f) * k;
    f = 2.0f * atanf(t);
    if (f > 2.8f) f = 2.8f;  // 160 deg at most
    if (f < 0.05f) f = 0.05f;
    memcpy(&bits, &f, 4);
    return bits;
}

// 4F70A0's replacement, on the game thread: remembers the camera's own angle so a menu change can apply at once.
// v3.3: the caller tells the camera: Room 302's first-person camera calls at 0x500829 and 0x500B9E (returns 0x50082E /
// 0x500BA3); the world rig at 0x50629D (Playable Eileen's over-the-shoulder Fov included) is the third-person one.
static DWORD __cdecl FovSetC(DWORD bits, DWORD ret)
{
    const int fp = ret == 0x50082E || ret == 0x500BA3;
    const DWORD out = FovBits(bits, fp);
    g_fovRawBits = bits;
    g_fovOutBits = out;
    g_fovLastFp = fp;
    return out;
}

static void SetFov(int idx, int save, int fp)
{
    char v[16];
    if (idx < 0 || idx >= FOV_N) idx = 0;
    InterlockedExchange(fp ? &g_fovFpSel : &g_fovSel, idx);
    if (!save) return;
    // v2.0 (user: "FOV ... does not immediately change in Room 302"): Room 302's first-person camera sets its angle
    // only when it starts, the world rig every frame. Re-apply the last angle now, unless something else (a cutscene)
    // has written [0xFD5B80] since.
    if (g_fovRawBits && g_fovLastFp == fp && *(volatile DWORD *)0xFD5B80 == g_fovOutBits) {
        const DWORD out = FovBits(g_fovRawBits, fp);
        g_fovOutBits = out;
        *(volatile DWORD *)0xFD5B80 = out;
    }
    sprintf_s(v, sizeof v, "%d", kFovDeg[idx]);
    if (!g_iniFallback) WritePrivateProfileStringA("Window", fp ? "FOVFirstPerson" : "FOV", v, g_iniPath);
    Log("field of view, %s: %s%s", fp ? "first person" : "third person", kFovName[idx], idx ? " degrees" : " (the game's own)");
}

static __declspec(naked) void hkFovSet(void)  // 4F70A0: void __cdecl (float fov); callers keep ecx / edx
{
    __asm {
        push ecx
        push edx
        push dword ptr [esp + 8]   // v3.3: the return address (who set it)
        push dword ptr [esp + 10h] // the angle
        call FovSetC
        add esp, 8
        mov dword ptr ds:[0FD5B80h], eax
        pop edx
        pop ecx
        ret
    }
}

// ---------------------------------------------------------------- v1.9 / v2.0: frame rate (experimental)
// Declared here, defined with the patch helpers below (FRAME RATE ENGINE).
static void FpsUserChanged(int n);
static void FpsCapRefresh(const char *why);  // v3.12.0
static int FpsIndexOf(int fps)
{
    int i;
    for (i = 0; i < FPS_N; ++i)
        if (kFps[i] == fps) return i;
    return 0;
}

static int __cdecl FpsGet(void) { return (int)g_fpsSel; }
static int __cdecl FpsDesc(void) { return MSG_FPS_DESC; }
static int __cdecl FpsInput(BYTE *cfg, int count)
{
    const int v = StepValue(FpsGet(), count);
    (void)cfg;
    if (v >= 0 && v < FPS_SHOWN) {  // v3.31 FPS331: never a hidden rate
        InterlockedExchange(&g_fpsSel, v);
        if (!g_iniFallback) WritePrivateProfileStringA("Window", "FrameRate", kFpsName[v], g_iniPath);
        FpsUserChanged(kFps[v]);  // v2.0: live, from the next gameplay frame (menus run at 30)
    }
    return 0;
}

// (v3.39 PR339: the v3.12.0 V-Sync / Anti-Aliasing / Anisotropic row functions are gone; Renderer / Opening: presentation.inc)

// v3.13.0: the Subtitle Style page's rows (live: applied and saved at once; snapshot / discard like FOV)
static void SubSet(int k, int v)
{
    if (k < 0 || k >= SUB_N || v < 0 || v >= kSubCount[k]) return;
    InterlockedExchange(&g_sub[k], v);
    if (!g_iniFallback) WritePrivateProfileStringA("Subtitles", kSubIniKey[k], kSubName[k][v], g_iniPath);
    Log("subtitles: %s %s", kSubIniKey[k], kSubName[k][v]);
}
static int SubInput(int k, int count)
{
    const int v = StepValue((int)g_sub[k], count);
    if (v >= 0) SubSet(k, v);
    return 0;
}
static int __cdecl SubIn0(BYTE *cfg, int count) { (void)cfg; return SubInput(0, count); }
static int __cdecl SubIn1(BYTE *cfg, int count) { (void)cfg; return SubInput(1, count); }
static int __cdecl SubIn2(BYTE *cfg, int count) { (void)cfg; return SubInput(2, count); }
static int __cdecl SubIn3(BYTE *cfg, int count) { (void)cfg; return SubInput(3, count); }
static int __cdecl SubIn4(BYTE *cfg, int count) { (void)cfg; return SubInput(4, count); }
static int __cdecl SubIn5(BYTE *cfg, int count) { (void)cfg; return SubInput(5, count); }
static int __cdecl SubGet0(void) { return (int)g_sub[0]; }
static int __cdecl SubGet1(void) { return (int)g_sub[1]; }
static int __cdecl SubGet2(void) { return (int)g_sub[2]; }
static int __cdecl SubGet3(void) { return (int)g_sub[3]; }
static int __cdecl SubGet4(void) { return (int)g_sub[4]; }
static int __cdecl SubGet5(void) { return (int)g_sub[5]; }
static int __cdecl SubDesc0(void) { return MSG_SUBD0 + 0; }
static int __cdecl SubDesc1(void) { return MSG_SUBD0 + 1; }
static int __cdecl SubDesc2(void) { return MSG_SUBD0 + 2; }
static int __cdecl SubDesc3(void) { return MSG_SUBD0 + 3; }
static int __cdecl SubDesc4(void) { return MSG_SUBD0 + 4; }
static int __cdecl SubDesc5(void) { return MSG_SUBD0 + 5; }
static int __cdecl SubDescBack(void) { return MSG_SUBD0 + SUB_N; }
// v3.16.0: a row input's return value for a page change: 4 = the stock sub-screen exit (fade, SubScreen at 0x526CD2),
// or (those hooks missing) 1 = the v3.15.0 slide mode 2 (Mode2Slide)
static int SubPageExit(void) { return g_subScreenOn ? 4 : 1; }
// Back: Enter fades (or slides) back to Option 3
static int __cdecl SubBackIn(BYTE *cfg, int count)
{
    (void)cfg;
    (void)count;
    if (!((Fn2)0x4F84A0)(0, 0x11)) return 0;
    InterlockedExchange(&g_subReq, 2);
    ((Fn1)0x56CC40)(0x9C42);  // the stock confirm sound (0x5261CB)
    return SubPageExit();
}
// Option 3 row 1 (Subtitles): Left / Right / arrow clicks as stock (0x524AF0); Enter opens the Subtitle Style page
static int __cdecl SubRowInput(BYTE *cfg, int count)
{
    if (g_subOn && ((Fn2)0x4F84A0)(0, 0x11)) {
        InterlockedExchange(&g_subReq, 1);
        ((Fn1)0x56CC40)(g_subScreenOn ? 0xA411 : 0x9C42);  // v3.16.0: the stock sub-screen sound (Button Config, 52495B)
        return SubPageExit();
    }
    return ((int(__cdecl *)(BYTE *, int))0x524AF0)(cfg, count);
}
static int __cdecl SubRowDesc(void) { return MSG_SUBROWD; }

// v3.14.0: Option 2 row 2 (Vibration): Off / Low / Medium / High. Mirrors the stock input 0x524980 (cfg+0x0B, the
// 0x9C43 sound through StepValue, the preview script 0x4F7E20(0x654AF8) when it turns on or changes level, stop-all
// 0x553CF0 when it turns off); the stock input also stops all on every idle frame, which cut its own preview short.
static void VibSetLevel(int v)
{
    if (v < 1 || v > 3) return;
    InterlockedExchange(&g_vibLevel, v);
    InterlockedExchange(&g_vibLogN, 4);
    if (!g_iniFallback) WritePrivateProfileStringA("Input", "Vibration", kVibName[v], g_iniPath);
    Log("vibration: %s", kVibName[v]);
}
static int __cdecl VibGet(void) { return CFG[0x0B] ? (int)g_vibLevel : 0; }
static int __cdecl VibDesc(void) { return MSG_VIBD; }
static int __cdecl VibIn(BYTE *cfg, int count)
{
    const int v = StepValue(VibGet(), count);
    if (v < 0) return 0;
    cfg[0x0B] = (BYTE)(v ? 1 : 0);
    if (v) {
        VibSetLevel(v);
        ((void(__cdecl *)(const void *))0x4F7E20)((const void *)0x654AF8);  // the stock preview
    } else {
        ((void(__cdecl *)(void))0x553CF0)();  // stop all (stock)
    }
    return 0;
}
static int VibChanged(void) { return g_snapOn && g_vibMenu && g_vibLevel != g_snapVib; }
static int SubChanged(void)
{
    int k;
    if (!g_snapOn) return 0;
    for (k = 0; k < SUB_N; ++k)
        if (g_sub[k] != g_snapSub[k]) return 1;
    return 0;
}

// v3.15.0: the Screen Effects page's rows (live: applied and saved at once; snapshot / discard like Subtitle Style)
static void FxSet(int k, int v)
{
    if (k < 0 || k >= FX_N || v < 0 || v >= kFxCount[k]) return;
    InterlockedExchange(&g_fx[k], v);
    if (!g_iniFallback) WritePrivateProfileStringA("ScreenEffects", kFxIniKey[k], kFxName[k][v], g_iniPath);
    Log("screen effects: %s %s", kFxIniKey[k], kFxName[k][v]);
}
static int FxInput(int k, int count)
{
    const int v = StepValue((int)g_fx[k], count);
    if (v >= 0) FxSet(k, v);
    return 0;
}
static int __cdecl FxIn0(BYTE *cfg, int count) { (void)cfg; return FxInput(0, count); }
static int __cdecl FxIn1(BYTE *cfg, int count) { (void)cfg; return FxInput(1, count); }
static int __cdecl FxIn2(BYTE *cfg, int count) { (void)cfg; return FxInput(2, count); }
static int __cdecl FxIn3(BYTE *cfg, int count) { (void)cfg; return FxInput(3, count); }
static int __cdecl FxIn4(BYTE *cfg, int count) { (void)cfg; return FxInput(4, count); }  // v3.22.0
static int __cdecl FxGet0(void) { return (int)g_fx[0]; }
static int __cdecl FxGet1(void) { return (int)g_fx[1]; }
static int __cdecl FxGet2(void) { return (int)g_fx[2]; }
static int __cdecl FxGet3(void) { return (int)g_fx[3]; }
static int __cdecl FxGet4(void) { return (int)g_fx[4]; }
static int __cdecl FxDesc0(void) { return MSG_FXD0 + 0; }
static int __cdecl FxDesc1(void) { return MSG_FXD0 + 1; }
static int __cdecl FxDesc2(void) { return MSG_FXD0 + 2; }
static int __cdecl FxDesc3(void) { return MSG_FXD0 + 3; }
static int __cdecl FxDesc4(void) { return MSG_FXD0 + 4; }
static int __cdecl FxDescBack(void) { return MSG_FXD0 + FX_N; }
// Back: Enter fades (or slides) back to Option 3, cursor on Noise Effect (request 4)
static int __cdecl FxBackIn(BYTE *cfg, int count)
{
    (void)cfg;
    (void)count;
    if (!((Fn2)0x4F84A0)(0, 0x11)) return 0;
    InterlockedExchange(&g_subReq, 4);
    ((Fn1)0x56CC40)(0x9C42);  // the stock confirm sound (0x5261CB)
    return SubPageExit();
}
// Option 3 row 2 (Noise Effect): Left / Right / arrow clicks go to the stock input kept from the copied row (0x524E40);
// Enter opens the Screen Effects page (request 3)
static DWORD g_fxStockIn = 0x524E40;
static int __cdecl FxRowInput(BYTE *cfg, int count)
{
    if (g_fxOn && ((Fn2)0x4F84A0)(0, 0x11)) {
        InterlockedExchange(&g_subReq, 3);
        ((Fn1)0x56CC40)(g_subScreenOn ? 0xA411 : 0x9C42);  // v3.16.0: the stock sub-screen sound
        return SubPageExit();
    }
    return ((int(__cdecl *)(BYTE *, int))(ULONG_PTR)g_fxStockIn)(cfg, count);
}
static int __cdecl FxRowDesc(void) { return MSG_FXROWD; }
static int FxChanged(void)
{
    int k;
    if (!g_snapOn) return 0;
    for (k = 0; k < FX_N; ++k)
        if (g_fx[k] != g_snapFx[k]) return 1;
    return 0;
}

static int __cdecl FovGet(void) { return (int)g_fovSel; }
static int __cdecl FovDesc(void) { return MSG_FOV_DESC; }
static int __cdecl FovInput(BYTE *cfg, int count)
{
    const int v = StepValue(FovGet(), count);
    (void)cfg;
    if (v >= 0) SetFov(v, 1, 0);
    return 0;
}
static int __cdecl ItemGet(void) { return g_sparkleOn ? 1 : 0; }  // v3.3: Item Sparkle, Off / On
static int __cdecl ItemDesc(void) { return MSG_ITEM_DESC; }
static int __cdecl ItemInput(BYTE *cfg, int count)
{
    const int v = StepValue(ItemGet(), count);
    (void)cfg;
    if (v >= 0) SparkleUserChanged(v);
    return 0;
}
// v3.7.0 Option 6 rows: [ExtraMenu] switches, saved at once (the pause menu reads them every frame)
static void EmSet(int k, int on)
{
    if (k < 0 || k >= EM_N) return;
    InterlockedExchange(&g_emOn[k], on ? 1 : 0);
    if (!g_iniFallback) WritePrivateProfileStringA("ExtraMenu", kEmKey[k], on ? "1" : "0", g_iniPath);
    Log("extra menu: row %s %s", kEmKey[k], on ? "on" : "off");
}
static int EmIn(int k, int count)
{
    const int v = StepValue(g_emOn[k] ? 1 : 0, count);
    if (v >= 0) EmSet(k, v);
    return 0;
}
#define EM_ROW(k)     static int __cdecl EmGet##k(void) { return g_emOn[k] ? 1 : 0; }     static int __cdecl EmDesc##k(void) { return MSG_EMD0 + k; }     static int __cdecl EmIn##k(BYTE *cfg, int count) { (void)cfg; return EmIn(k, count); }
EM_ROW(0) EM_ROW(1) EM_ROW(2) EM_ROW(3) EM_ROW(4) EM_ROW(5)
#undef EM_ROW
// v3.8.0 Save Banks rows. SbSetBank is also called by the save / load lists' Q / E (savebanks.inc): one value.
static void SbSetOn(int on)
{
    char v[8];
    InterlockedExchange(&g_sbOn, on ? 1 : 0);
    sprintf_s(v, sizeof v, "%d", on ? 1 : 0);
    if (!g_iniFallback) WritePrivateProfileStringA("SaveBanks", "Enabled", v, g_iniPath);
    Log("save banks: %s", on ? "on" : "off (the game's own sh4.sav only)");
}
static int g_sbListStep;            // v3.48 SB348: 1 = a step in the save / load list (savebanks.inc sb_flip)
static volatile LONG g_sbQStep;     // v3.48 SB348: its verbose lines this session
static void SbSetBank(int bank)
{
    char v[8];
    if (bank < 1) bank = 1;
    if (bank > SB_MAX) bank = SB_MAX;
    if (bank == g_sbBank) return;
    InterlockedExchange(&g_sbBank, bank);
    sprintf_s(v, sizeof v, "%d", bank);
    if (!g_iniFallback) WritePrivateProfileStringA("SaveBanks", "Bank", v, g_iniPath);
    if (!g_sbListStep) Log("save banks: bank %d", bank);  // v3.48 SB348: Option 6's row, the Options undo
    else if (LogQuota(&g_sbQStep, 100)) LogV("save banks: bank %d", bank);  // the list: savebanks.inc logs the settled change
}
static int __cdecl SbOnGet(void) { return g_sbOn ? 1 : 0; }
static int __cdecl SbOnDesc(void) { return MSG_SB_DESC; }
static int __cdecl SbOnIn(BYTE *cfg, int count)
{
    const int v = StepValue(g_sbOn ? 1 : 0, count);
    (void)cfg;
    if (v >= 0) SbSetOn(v);
    return 0;
}
static int __cdecl SbBankGet(void) { return g_sbOn ? (int)g_sbBank - 1 : SB_MAX; }  // Off: "-" (SB-002)
static int __cdecl SbBankDesc(void) { return MSG_SBB_DESC; }
static int __cdecl SbBankIn(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    if (!g_sbOn) return 0;  // review SB-002: no bank choice while banks are Off
    (void)count;  // the row declares SB_MAX + 1 values ("-" for Off); a bank steps over 1..SB_MAX only
    v = StepValue((int)g_sbBank - 1, SB_MAX);
    if (v >= 0) SbSetBank(v + 1);
    return 0;
}
static int __cdecl FovFpGet(void)  // v3.3; v3.3.5: the position of g_fovFpSel in the row's own list (kFovFpIdx)
{
    int r;
    for (r = 0; r < FOVFP_N; ++r)
        if (kFovFpIdx[r] == (int)g_fovFpSel) return r;
    return 0;
}
static int __cdecl FovFpDesc(void) { return MSG_FOVFP_DESC; }
static int __cdecl FovFpInput(BYTE *cfg, int count)
{
    const int v = StepValue(FovFpGet(), count);
    (void)cfg;
    if (v >= 0 && v < FOVFP_N) SetFov(kFovFpIdx[v], 1, 1);
    return 0;
}

// v3.3.5 VIEW GUARD (user: "In Room 302, when the player looks out of the window (the window-viewing fixed view), a
// custom FOV lets the player see past the window frame's edge. Detect the window-viewing state and use the game's
// default FOV during it (likewise for the peephole view)"). Room 302's views are event records, run from the running
// event list 0xFD198C (sentinel node; node +4 next, the game's end test masks 0x4FFFFFFF; node +8 instance, instance +0
// = its record, as 0x4CFFC0 compares it):
//   0x69843C "window, look out" (trigger 3; init 0x54C050, update 0x54C190, enters game phase 1 through 0x4CCBF0;
//            its camera 0x54BE40 sets the render camera itself, 0x54BFE0, and its own angle, 45 deg at 0x54C002),
//   0x61C3FC "The window... I can't get it open..." (the examine objects on the windows: the first-person camera
//            stays on the window with [0xFD5B80], the angle this mod scales),
//   0x61B790 the door scope (0x4D8000; its look task sets 60 deg at 0x4D848B), 0x61B6B8 / 0x61B86C the story peephole
//            views, 0x61CBF4 "The hole is blocked..." (examine).
// Before the event starts, the view turns onto the pressed look-at target: [0x13D7B14] = 1 with the target object in
// [0x13D7B08] (its record = [[obj+0x408]+0]). While any of these runs or is being turned to, the first-person angle
// eases to the camera's own (0.3 s) and back afterwards. [0xFD5B80] is only touched while it still holds the value
// this mod stored (a cutscene's own angle is left alone). Polled from the HUD call 0x4EFBFD (items.inc), every frame.
// v3.41 VW341: + 0x698468, the look-out's second record (the same five handlers as 0x69843C; four event tables name it)
static const DWORD kFovViewRecs[] = { 0x69843C, 0x61C3FC, 0x61B790, 0x61B6B8, 0x61B86C, 0x61CBF4, 0x698468 };
static float g_fovViewS = 1.0f;  // 1 = the chosen first-person angle, 0 = the camera's own
static int g_fovViewLast = -1;
static DWORD g_fovViewWrote;  // g_fovOutBits as this guard last left it
static LARGE_INTEGER g_fovViewT;

static int FovViewRec(DWORD rec)
{
    int k;
    for (k = 0; k < (int)(sizeof kFovViewRecs / sizeof kFovViewRecs[0]); ++k)
        if (kFovViewRecs[k] == rec) return 1;
    return 0;
}

static DWORD FovViewNow(void)  // the view record that runs or is being turned to, or 0
{
    const BYTE *n = *(BYTE *const *)(0xFD198C + 4);
    int guard = 0;
    while (((DWORD)(ULONG_PTR)n & 0x4FFFFFFF) && guard++ < 64) {
        const BYTE *inst = *(BYTE *const *)(n + 8);
        if (inst && FovViewRec(*(const DWORD *)inst)) return *(const DWORD *)inst;
        n = *(BYTE *const *)(n + 4);
    }
    if (*(volatile DWORD *)0x13D7B14 == 1) {
        const BYTE *obj = *(BYTE *const *)0x13D7B08;
        if (obj && TaskSlotOk(obj)) {  // v3.36 TP336: a slot of the task pool in use (pools_sim.h)
            const BYTE *ev = *(BYTE *const *)(obj + 0x408);
            if (ev && FovViewRec(*(const DWORD *)ev)) return *(const DWORD *)ev;
        }
    }
    return 0;
}

static void FovViewPollC(void)
{
    LARGE_INTEGER now, f;
    float dt = 0.0f, target;
    DWORD stage, out, rec;
    QueryPerformanceCounter(&now);
    if (g_fovViewT.QuadPart && QueryPerformanceFrequency(&f) && f.QuadPart)
        dt = (float)((double)(now.QuadPart - g_fovViewT.QuadPart) / (double)f.QuadPart);
    g_fovViewT = now;
    if (!(dt >= 0.0f) || dt > 0.1f) dt = 0.1f;
    stage = *(volatile DWORD *)0xFD5A90;
    if (!g_fovRow || !g_fovLastFp || !g_fovRawBits || (stage != 1 && stage != 0x0A)) {
        g_fovViewS = 1.0f;  // not Room 302's first-person camera: FovSetC applies the chosen angle as before
        g_fovViewLast = -1;
        return;
    }
    rec = FovViewNow();
    if ((int)(rec != 0) != g_fovViewLast) {
        if (rec)
            Log("field of view: Room 302 view (event record %06lX, window or peephole): the game's own angle", rec);
        else if (g_fovViewLast > 0)
            Log("field of view: Room 302 view over: first person %s%s again", kFovName[g_fovFpSel],
                g_fovFpSel ? " degrees" : "");
        g_fovViewLast = rec != 0;
    }
    target = rec ? 0.0f : 1.0f;
    if (g_fovOutBits != g_fovViewWrote && !rec) g_fovViewS = 1.0f;  // the camera set its angle again (FovSetC): as is
    if (g_fovViewS < target) g_fovViewS = g_fovViewS + dt / 0.3f > target ? target : g_fovViewS + dt / 0.3f;
    else if (g_fovViewS > target) g_fovViewS = g_fovViewS - dt / 0.3f < target ? target : g_fovViewS - dt / 0.3f;
    if (*(volatile DWORD *)0xFD5B80 != g_fovOutBits) return;  // written by something else since: leave it
    if (g_fovViewS >= 1.0f) {
        out = FovBits(g_fovRawBits, 1);
    } else if (g_fovViewS <= 0.0f || g_fovFpSel == 0) {
        out = g_fovRawBits;
    } else {
        float a;
        memcpy(&a, &g_fovRawBits, 4);
        if (!(a > 0.01f && a < 3.0f)) return;
        a = 2.0f * atanf(tanf(0.5f * a) * (1.0f + (FovFactor(1) - 1.0f) * g_fovViewS));
        if (a > 2.8f) a = 2.8f;
        if (a < 0.05f) a = 0.05f;
        memcpy(&out, &a, 4);
    }
    if (out != g_fovOutBits) {
        g_fovOutBits = out;
        *(volatile DWORD *)0xFD5B80 = out;
    }
    g_fovViewWrote = g_fovOutBits;
}

static void FovViewPoll(void)  // game thread, every frame (the HUD call hook in items.inc)
{
    static int faults;
    if (faults > 3) return;
    __try {
        FovViewPollC();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (++faults == 1) Log("field of view: the Room 302 view guard faulted; it stops after 3 more");
    }
}

// v1.5: the wrapper reads dxcfg.ini again at the device reset that the size re-apply makes; the Watch thread then
// gives the window the mode's frame (ReshapeWindow, woken by PickSize).
// The mode is also kept in SH4Refined.ini [Window] Borderless: the game saves Options changed during play only into
// the loaded game's save bank (sh4_bankN.sav), but reads sh4.sav at start.
static void SetDisplayMode(int m)
{
    if (m == g_dm) return;
    InterlockedExchange(&g_dm, m);
    ForceReapply();
    if (!g_iniFallback) WritePrivateProfileStringA("Window", "Borderless", m == DM_BORDERLESS ? "1" : "0", g_iniPath);
    Log("display mode: %s", kDmName[m]);
}

// v1.7: the GOG wrapper's desktop size. Its DllMain reads the PRIMARY monitor's mode once (EnumDisplaySettingsA(NULL,
// ENUM_CURRENT_SETTINGS), ori_d3d8.dll RVA 17BB6) into two globals; its config reader (RVA 1BB60, at every CreateDevice
// and Reset) makes them mode-table entry 0, which dxcfg display=desktop (Borderless) takes as the back buffer size. On a
// monitor of another size Borderless would letterbox for the primary's shape and be stretched onto the chosen one, so
// the two globals are set to the chosen monitor's mode (only when they differ). Both instructions are checked first
// (RVA 17BE9 `mov [W], eax` / 17BEE `mov [H], ecx` in DllMain and 1BD1F `mov ecx, [W]` / 1BD29 `mov edx, [H]` in the
// reader must name the same two addresses inside the module); any other wrapper build is left alone. The wrapper's
// other uses of them (window or popup, placement at 0,0) only reach SetWindowLong / SetWindowPos, which the mod
// overrides anyway.
static DWORD *g_wdW, *g_wdH, g_wdOrigW, g_wdOrigH;
static int g_wdState;  // 0 not looked for yet, 1 found, -1 not this wrapper

// v1.9 (audit INT-5): another d3d8.dll (dgVoodoo2, d3d8to9, a packed build) may have no readable bytes at these RVAs:
// the compares run under SEH, and a fault counts as "not GOG's wrapper".
static int WrapperBytesOk(const BYTE *b, const BYTE *kMain, size_t nMain, const BYTE *kRead, size_t nRead, DWORD *w, DWORD *h)
{
    __try {
        if (memcmp(b + 0x17BC2, kMain, nMain) || b[0x17BE9] != 0xA3 || b[0x17BEE] != 0x89 || b[0x17BEF] != 0x0D ||
            b[0x1BD1F] != 0x8B || b[0x1BD20] != 0x0D || memcmp(b + 0x1BD25, kRead, nRead))
            return 0;
        *w = *(const DWORD *)(b + 0x17BEA);
        *h = *(const DWORD *)(b + 0x17BF0);
        return *w == *(const DWORD *)(b + 0x1BD21) && *h == *(const DWORD *)(b + 0x1BD2B);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

static int FindWrapperDesktop(void)
{
    static const char *const names[] = { "ori_d3d8.dll", "d3d8.dll" };
    static const BYTE kMain[] = { 0x8B, 0x44, 0x24, 0x6C, 0x8B, 0x4C, 0x24, 0x70, 0x8B, 0x54, 0x24, 0x78, 0xA3 };  // RVA 17BC2
    static const BYTE kRead[] = { 0x33, 0xDB, 0x89, 0x08, 0x8B, 0x15 };                                           // RVA 1BD25
    int i, any = 0;
    for (i = 0; i < 2; ++i) {
        BYTE *b = (BYTE *)GetModuleHandleA(names[i]);
        IMAGE_NT_HEADERS *nt;
        DWORD size, w, h;
        if (!b) continue;
        any = 1;
        nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
        size = nt->OptionalHeader.SizeOfImage;
        if (size < 0x1BD30) continue;
        if (!WrapperBytesOk(b, kMain, sizeof kMain, kRead, sizeof kRead, &w, &h)) continue;
        if (w < (DWORD)(ULONG_PTR)b ||
            w + 4 > (DWORD)(ULONG_PTR)b + size || h < (DWORD)(ULONG_PTR)b || h + 4 > (DWORD)(ULONG_PTR)b + size)
            continue;
        g_wdW = (DWORD *)(ULONG_PTR)w;
        g_wdH = (DWORD *)(ULONG_PTR)h;
        g_wdOrigW = *g_wdW;
        g_wdOrigH = *g_wdH;
        Log("monitor: GOG wrapper %s found (desktop size %lux%lu at %p / %p)", names[i], g_wdOrigW, g_wdOrigH, g_wdW, g_wdH);
        return 1;
    }
    if (!any) return 0;  // not loaded yet: look again next time
    Log("monitor: GOG wrapper not recognised: Borderless keeps the primary monitor's desktop size as its picture size");
    return -1;
}

static void WrapperDesktop(void)  // game thread: before CreateDevice and before the reset a monitor change makes
{
    DWORD w, h;
    if (!g_enable || !g_fitWrapper || g_monN < 2) return;
    if (!g_wdState) g_wdState = FindWrapperDesktop();
    if (g_wdState <= 0) return;
    if (!MonModeSize(g_monSel, &w, &h)) {
        w = g_wdOrigW;
        h = g_wdOrigH;
    }
    if (*g_wdW != w || *g_wdH != h) {
        *g_wdW = w;
        *g_wdH = h;
        Log("monitor: the wrapper's desktop size (Borderless picture) is now %lux%lu, %s", w, h, MonLabel(g_monSel));
    }
}

// v1.7: a new monitor (Options, or the window dragged there): remembered in the ini; the Watch thread moves the window
// (ReshapeWindow); in Borderless the size re-apply makes a device reset, so the wrapper's back buffer takes its size.
static void SetMonitor(int m, const char *why)
{
    if (m < 0 || m >= g_monN || m == g_monSel) return;
    InterlockedExchange(&g_monSel, m);
    MonSave(m);
    WrapperDesktop();
    Log("monitor: %s (%s), %s", MonLabel(m), g_mons[m].dev, why);
    FpsCapRefresh("monitor");  // v3.12.0: its refresh may differ
    if (g_dm == DM_BORDERLESS) ForceReapply();
    if (g_reshapeEvt) SetEvent(g_reshapeEvt);
}

// (v3.39 PR339: v3.12.0's GfxApply is gone: no Option 4 graphics rows; the V-Sync cap reads dxcfg.ini itself)

// 5242A0 SetResolutionIndex(entry): boot / load / discard (524320) and Options exit with "keep" (5268FC). The stock
// part is gated (only when the Resolution row changed); the shape is checked on every call.
static void __cdecl SetResIndex(int idx)
{
    BYTE *cfg = CFG;
    if (!g_inDiscard) PresApply();  // v3.39 PR339: Renderer / Opening at "Yes" (no-op at boot / load: rows == snapshots; was GfxApply)
    if ((*(int *)0x1092220 || *(int *)0x654AF0) && (unsigned)idx < 6) *(int *)0x610DD4 = idx;
    if (!g_bootDone && *(int *)0x1092248) cfg[0x21] = 1 + ASP_43;  // SafeMode start: the game resets to 1024x768
    g_bootDone = 1;
    if (cfg[0x21] < 1 || cfg[0x21] > ASP_N) cfg[0x21] = (BYTE)(1 + (cfg[0x20] >= 2 ? ASP_169 : ASP_43));
    if (cfg[0x21] - 1 != g_applied) {
        g_applied = cfg[0x21] - 1;
        ForceReapply();  // [0x610DD8] != [0x610DD4]: the main tick re-applies the size (4148E0), device reset included
    }
    if (g_dispRow) {  // v1.5: at start the ini decides
        if (!g_dmBooted) {
            g_dmBooted = 1;
            cfg[0x22] = (BYTE)(1 + (g_borderlessIni ? DM_BORDERLESS : DM_WINDOWED));
        }
        if (cfg[0x22] < 1 || cfg[0x22] > DM_N) cfg[0x22] = 1 + DM_WINDOWED;
        g_dmApplied = cfg[0x22] - 1;
        SetDisplayMode(g_dmApplied);
    }
    if (g_monRow) {  // v1.7: likewise the monitor (at start the ini's choice; later the row's)
        if (!g_monBooted) {
            g_monBooted = 1;
            cfg[0x23] = (BYTE)(1 + g_monSel);
        }
        if (cfg[0x23] < 1 || cfg[0x23] > g_monN) cfg[0x23] = (BYTE)(1 + g_monSel);
        g_monApplied = cfg[0x23] - 1;
        SetMonitor(g_monApplied, "Options");
    }
}

// 570D11 / 571DED (loading a game, both load paths): `call 524630` copies the slot's Options block into the live one,
// then the setter (5242A0, via 524320) runs. The shape byte rode along with the block and the setter adopted it, so
// loading a save made in another shape switched the picture back (user, 2026-10-29: "had to set the aspect ratio again in game").
// v1.4.1: after the copy, the live size entry and shape are put back: both are display settings, not per save.
static int __cdecl LoadOptions(void *slotOptions)
{
    const int entry = *(int *)0x610DD4, r = ((int(__cdecl *)(void *))0x524630)(slotOptions);
    if ((unsigned)entry < 6) CFG[0x20] = (BYTE)entry;
    if (g_applied >= 0 && g_applied < ASP_N) CFG[0x21] = (BYTE)(g_applied + 1);
    if (g_dispRow && g_dmApplied >= 0) CFG[0x22] = (BYTE)(g_dmApplied + 1);  // v1.5
    if (g_monRow) CFG[0x23] = (BYTE)(g_monSel + 1);                           // v1.7
    return r;
}

// The selected row's right arrow sits at x 464; GOG moves it 20 further for Resolution labels longer than 8
// characters (draw 52587C, mouse rect 5252B0 via 525D04). v1.5: the same rule follows Resolution to row 1, and the
// Display Mode row moves it too ("Windowed" 20, "Borderless" 36). v1.6: the rule is the label's real length (GOG
// tested entry > 2; 21:9 has a 9-character label at entry 2), and the two hooks are in also without Display Mode.
// v1.7: user, "The right arrow shifts to the right when switching to Borderless from Windowed mode": the Display Mode
// branch gave 20 for "Windowed" and 36 for "Borderless", so that arrow jumped 16 to the right with the value. Our rows
// now keep one shift per row, the one their longest label needs (Display Mode 36, Monitor 20 for "Display n"), so
// their arrows stay put; the Resolution row keeps GOG's own per-label rule (its stock look).
static int ShiftFor(int chars) { return chars > 9 ? 36 : chars > 8 ? 20 : 0; }
// v3.5.0: the logical Options page (0..LP_N-1) and the identity the stock page checks see. Set only by SetLogicalPage
// (the page machinery below).
static volatile int g_lpage, g_stockView;
// v3.5.0: the cursor of the page on screen comes from its own physical widget (review OPT-PAGE-001): the copy +0x30
// that 525B2C refreshes every frame for the active page (0x1091A50 is only slot 0's).
static int ActiveCursor(void)
{
    const int slot = *(int *)0x1091B1C;
    return (unsigned)slot < 3 ? *(int *)(0x1091A20 + 0x38 * slot + 0x30) : 0;
}
// v3.19.0 C2b: the width of a long value on (logical page, row), 0 = not long; measured with the game's font metrics
// (LabelW; live: 10.4 px a character left the arrow on "Mono")
static float LabelW(const BYTE *s);
static int GpLongValW(int page, int row);  // v3.31 ENEMIES: gameplay.inc (Enemies Follow's long value)
#include "arrowfit_sim.h"  // v3.33 MENU333: AfRowShift / AF_COLW (test\arrowfit_test.c)
static int RowValWidths(int page, int row, float *w, int max, float *cur);  // v3.33 MENU333
static int LongValW(int page, int row)
{
    __try {
        {   // v3.31 ENEMIES
            const int g = GpLongValW(page, row);
            if (g) return g;
        }
        {  // v3.33 CAM333: "Playable Eileen" on Option 6's Camera row
            const int c = CamLongValW(page, row);
            if (c) return c;
        }
        if (page == 3 && row >= 0 && row == g_presMon && g_monLongW) {
            int a, w = 0;
            for (a = 0; a < g_monN; ++a) {
                const int x = (int)(LabelW(g_txtMons[a]) + 0.5f);
                if (x > w) w = x;
            }
            return w;
        }
        if (page == LP_FX && row == FX_FILTER) {
            const int v = (int)g_fx[FX_FILTER];
            return (int)strlen(kFxName[FX_FILTER][v]) > LONGVAL_CHARS ? (int)(LabelW(g_txtFxV[FX_FILTER][v]) + 0.5f) : 0;
        }
        {   // v3.33 MENU333: any other value wider than the column's room (AF_COLW): the column moves for it too
            float w[64], now;
            if (RowValWidths(page, row, w, 64, &now) && now > (float)AF_COLW) return (int)(now + 0.5f);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return 0;
}
static int LongMove(int w) { return w - 130 > LONGVAL_MOVE ? w - 130 : LONGVAL_MOVE; }  // the arrow ends by x 508
static int ArrowShiftOld(int cur, int entry);
static int ArrowShift(int page)
{
    const int cur = ActiveCursor(), entry = (signed char)CFG[0x20];
    const int lw = LongValW(g_lpage, cur);
    const int old = ArrowShiftOld(cur, entry);
    (void)page;  // the stock page id (g_stockView) is 7 on our pages: the logical page decides
    if (lw) return lw - 100 + 6;  // v3.19.0 C2b: the column moved LongMove left; the arrow 6 px after the text
    if (g_lpage == 0 && cur == 0) return old;  // v3.33 MENU333: Resolution keeps GOG's own per-label rule
    __try {  // v3.33 MENU333: every row measured (Echo Doubles "Shadow Cast" ran under the arrow)
        float w[64], now;
        const int n = RowValWidths(g_lpage, cur, w, 64, &now);
        const int s = n ? AfRowShift(w, n, old) : old;
        static LONG s_q;
        static int s_lastPage = -1, s_lastRow = -1;
        if (n && (g_lpage != s_lastPage || cur != s_lastRow)) {
            s_lastPage = g_lpage;
            s_lastRow = cur;
            if (s != old && LogQuota(&s_q, 200))
                LogV("options: page %d row %d: the right arrow %+d px (the widest value fits the column: %d values; "
                     "the old rule %+d)", g_lpage, cur, s, n, old);
        }
        return s;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static LONG s_f;
        if (LogQuota(&s_f, 3)) LogAnomaly("options", "measuring the values of page %d row %d faulted: old arrow rule", g_lpage, cur);
    }
    return old;
}
// the rules before v3.33 (by row, mostly by character count); the minimum ArrowShift keeps
static int ArrowShiftOld(int cur, int entry)
{
    {  // v3.33 CAM333: the Camera page's Side row, Option 6's Camera row (v3.33 MERGE: in the old rule, the minimum)
        const int c = CamArrowShift(g_lpage, cur);
        if (c >= 0) return c;
    }
    if (g_lpage == 0)  // Option 1: the Resolution row (stock row 0)
        return cur == 0 && entry >= 0 && entry < 6 && g_resLen[LiveAspect()][entry] > 8 ? 20 : 0;
    if (g_lpage == 3) {  // Option 4 (Presentation)
        if (cur == g_presDisp) return g_dmShift;
        if (cur == g_presMon) return g_monShift;
    }
    if (g_lpage == LP_SUB && (cur == SUB_EDGE || cur == SUB_BOX)) return 36;  // v3.13.0: "Outline+Shadow", "Translucent"
    if (g_lpage == LP_SUB && (cur == SUB_EFFECT || cur == SUB_EXIT))  // v3.20.0: "Typewriter", "Paper Peel"
        return ShiftFor((int)strlen(kSubName[cur][g_sub[cur]]));
    if (g_lpage == LP_FX && cur == FX_NOISE) return 36;  // v3.15.0: "Very High"
    if (g_lpage == LP_FX && (cur == FX_NTYPE || cur == FX_NCOLOUR))  // v3.18.0: "Scan Roll", "Full Colour"
        return strlen(kFxName[cur][g_fx[cur]]) > 8 ? 36 : 0;
    if (g_lpage == LP_FX && cur == FX_FILTER)  // v3.16.0: "Night Vision" / "Ghost Vision" (12) as v3.15.0's "High Contrast"; "Night Vision Mono" more
        return strlen(kFxName[FX_FILTER][g_fx[FX_FILTER]]) > 14 ? 60 : 36;
    return 0;
}
// 5252B0 (the mouse rects 525D04, its only caller: 1 = `add eax, esi` with esi = 14h on the right arrow's rect x).
// v3.33 MENU333: the rect follows the drawn arrow exactly: 1 with esi = the shift (esi is cleared at 525DA4 after)
static int __cdecl ArrowShiftMouse(void) { return ArrowShift(0); }
static __declspec(naked) void ArrowShifted(void)
{
    __asm {
        push ecx
        push edx
        call ArrowShiftMouse
        pop edx
        pop ecx
        test eax, eax
        jz none
        mov esi, eax
        mov eax, 1
    none:
        ret
    }
}
static const DWORD g_backArrow = 0x525899;
static __declspec(naked) void hkArrowDraw(void)  // 52587C: esi page, ebp arrow x, [esp+38h] its copy
{
    __asm {
        push eax
        push ecx
        push edx
        push esi
        call ArrowShift
        add esp, 4
        add ebp, eax
        mov [esp + 38h + 0Ch], ebp
        pop edx
        pop ecx
        pop eax
        jmp dword ptr [g_backArrow]
    }
}

// ================================================================ v3.5.0: Options 4 (Presentation), 5 (Gameplay), 6
// Plan and review: staging/options_pages/PLAN.md (claudex-loop, Codex APPROVED in round 4). The menu object 0x1091A20
// has room for three page widgets (+0 / +38h / +70h; the live cfg starts at +B0h) and every per-page table is three
// entries long, so the pages are virtual: logical page L lives in physical slot L mod 3. The slide (526210) goes from
// slot [obj+FC] to slot [obj+100]; with LP_N = 5 or 6 the page on screen and both neighbours are always in different
// slots (LP_N mod 3 != 1), so the target slot is never on screen when LoadSlot refills it just before the slide.
//   525B7A  page switch (pad 1000h / 4000h, the top arrows): target page, LoadSlot, [obj+100] / [obj+104], return 2
//   526439  slide commit (FC = [obj+100]): SetLogicalPage(pending target)
//   5262B2  slide mode 2 (unreachable in GOG v7: no row input returns 1; guarded): the target slot gets its stock page
//   523E4B  Options init (call 523D70): stock tables back in all three slots, SetLogicalPage(0)
//   13 reads of the current page [0x1091B1C] in the stock special cases now read g_stockView (L for 0-2, else 7)
// The change scan 523D20 walks pages 0-2 with the stock counts 0x655034 (never changed): it asks about stock rows only.
#define LP_N 6  // v3.7.0: Option 6 (Extra Menu) is in the cycle (6 mod 3 = 0 is valid; 4 and 7 are not)
typedef struct { int x, y, w, h, a, b; } RowRect;
static DWORD *g_lpTable[LP_MAX];  // v3.13.0 (review SUB-001): + the hidden Subtitle Style page; v3.15.0: + Screen Effects
static int g_lpCount[LP_MAX];
static DWORD g_lpTitle[LP_MAX];
static int g_lcursor[LP_MAX];
// v3.33 MENU333: the widths (LabelW, canvas px) of every value of (logical page, row), from the row's own table (the
// game's OptionsRow: +4 getter, +0xC value ids, +0x10 count, +0x14 label); 0 values for a bar, a button or an empty row.
// cur: the current value's width (-1 = not known).
static int RowValWidths(int page, int row, float *w, int max, float *cur)
{
    const DWORD *r;
    const DWORD *ids;
    int n, i, v;
    *cur = -1.0f;
    if ((unsigned)page >= LP_MAX || !g_lpTable[page] || (unsigned)row >= (unsigned)g_lpCount[page]) return 0;
    r = g_lpTable[page] + 7 * row;
    ids = (const DWORD *)(ULONG_PTR)r[3];
    n = (int)r[4];
    if (!ids || n <= 0 || n > max || r[5] == 0x25 || r[5] == 0x26 || !r[1]) return 0;
    v = ((int(__cdecl *)(void))(ULONG_PTR)r[1])();
    for (i = 0; i < n; ++i) {
        w[i] = LabelW(((const BYTE *(__cdecl *)(int, int))0x50C440)(0, (int)ids[i]));
        if (i == v) *cur = w[i];
    }
    return n;
}
// ---------------------------------------------------------------- v3.19.0 C3: submenu arrows (staging/menus_v319)
// The game's own right arrow (0x523620, dir 3, size 1.0) after the label of every row that opens a sub-screen:
// Option 1 Brightness / Gamma, Option 2 Button Config, Option 3 Subtitles / Noise Effect (when Refined's pages are in).
// Position: the game's row rect (0x552270: the REST rect; v3.21.0 adds 525990's slide offset) + the label width estimated from its byte-mode string.
// v3.19.0: the label's width from the game's own font metrics (layout 0x557660 at 0x5577CE: font = [0x1261B98 + 4 font],
// glyph i = 0xE0 - byte: width byte [font + 0x10 + i] + two signed kerning nibbles [font + 0xF0 + i]; space = [font + 0x10];
// + the letter spacing [0x1261C20] (2.5); 640-wide font units, canvas px = units / 1.25). Review MENU-ARROW-001: 00 80
// header, 00 = space, FF FF end, escapes. Live: a flat 10.4 px a character put the arrow into "Gamma".
static float LabelW(const BYTE *s)
{
    const BYTE *font = *(const BYTE *const *)0x1261B98;
    const float sp = *(volatile float *)0x1261C20 > 0.0f && *(volatile float *)0x1261C20 < 20.0f ? *(volatile float *)0x1261C20 : 2.5f;
    float u = 0.0f;
    int i, n = 0;
    if (!s) return 0.0f;
    i = s[0] == 0x00 && s[1] == 0x80 ? 2 : 0;
    while (n < 64) {
        const BYTE c = s[i];
        if (c == 0xFF) {
            if (s[i + 1] == 0xFF) break;
            i += 2;  // a control: nothing drawn
            continue;
        }
        if (c == 0xE0) u += 13.0f + sp, i += 3;  // a u16 character: about one letter
        else if (c == 0xF0) u += 35.0f, i += 2;  // an inline icon (advance 0x23)
        else if (c == 0xFC) u -= s[i + 1], i += 2;
        else if (c == 0xFE) u += s[i + 1], i += 2;
        else {
            if (!font) u += 13.0f + sp;
            else if (c == 0x00) u += font[0x10] + sp;
            else if (c <= 0xE0) {
                const int g = 0xE0 - c, k = font[0xF0 + g];
                int lo = k & 0x0F, hi = k >> 4;
                if (lo >= 8) lo -= 16;
                if (hi >= 8) hi -= 16;
                u += (float)(font[0x10 + g] + lo + hi) + sp;
            } else {
                u += 13.0f + sp;
            }
            ++i;
        }
        ++n;
    }
    return u / 1.25f;
}
// v3.22.0 (user item 15): the rows that open a sub-screen (the v3.19 arrow rule)
static int SubmenuRow(int slot, int row)
{
    int page;
    if ((unsigned)slot >= 3) return 0;
    page = g_slotPage[slot];
    if (page == 0) return row == 1 || row == 2;           // Brightness, Gamma
    if (page == 1) return row == 0;                       // Button Config
    if (page == 2) return (row == 1 && g_subMenu) || (row == 2 && g_fxMenu) || (row == 6 && ShMenuOn()) || (row == 5 && DofMenuOn());  // Subtitles, Noise Effect; v3.40 SH340: Realtime Shadow; v3.43 DF343: Hide Icon
    return GpSubmenuRow(page, row);  // v3.25.0: Option 5's Player / Enemies
}
// v3.22.0 (user item 15): no white arrows any more; a colour cue instead (RowLabelC). Kept behind g_subArrows.
static int g_subArrows = 0;
static void SubmenuArrow(int row, int slot, const BYTE *obj, int xoff)
{
    int page, mark = 0;
    if (!g_subArrows || (unsigned)slot >= 3 || !obj) return;
    page = g_slotPage[slot];
    if (page == 0) mark = row == 1 || row == 2;           // Brightness, Gamma
    else if (page == 1) mark = row == 0;                  // Button Config
    else if (page == 2) mark = (row == 1 && g_subMenu) || (row == 2 && g_fxMenu) || (row == 6 && ShMenuOn()) || (row == 5 && DofMenuOn());  // Subtitles, Noise Effect; v3.40 SH340: Realtime Shadow; v3.43 DF343: Hide Icon
    if (!mark || !g_lpTable[page] || row >= g_lpCount[page]) return;
    __try {  // (the font table read inside LabelW is guarded here too)
        RowRect r;
        const DWORD id = g_lpTable[page][row * 7 + 5];
        const BYTE *s = ((const BYTE *(__cdecl *)(int, int))0x50C440)(0, (int)id);
        const float w = LabelW(s);
        memset(&r, 0, sizeof r);
        ((void *(__cdecl *)(RowRect *, const void *, int))0x552270)(&r, obj + 0x38 * slot, row);
        if (r.w <= 0) return;
        // v3.21.0: the rect is the rest rect; 525990 adds its slide offset (arg5) to every row it draws, so add it here
        // too (the arrows stood still while their page slid away). Off the 512 canvas: nothing to draw.
        if (r.x + xoff + (int)w + 10 < -16 || r.x + xoff + (int)w + 10 > 528) return;
        ((void(__cdecl *)(float, float, int, float, DWORD))0x523620)((float)(r.x + xoff) + w + 10.0f, (float)r.y + 4.0f, 3, 1.0f,
            slot == *(volatile int *)0x1091B1C ? 0x80808080u : 0x80505050u);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        static int s_logged;
        if (!s_logged++) Log("options: a submenu arrow draw faulted (logged once)");
    }
}
// ---------------------------------------------------------------- v3.22.0 Options text (staging/feedback_v322 PLAN 3)
// Item 15: the label of a row that opens a sub-screen is drawn bright blue, deep blue under the cursor (the label call
// 0x525553 `call 5239A0`: eax flash count, esi label id, ebx y, [esp+4] x, [esp+8] the palette index (0 white, 2 the
// cursor's red, 4 changed yellow)); v3.23.0: the flash path (eax != 0) too, in the deep blue (user item 6).
// Item 16: on the Screen Effects page the menu text's shadow string (`call 50B580` after 556A50(8): 0x523A3A labels /
// values, 0x52418D the description, 0x5259C3 the page title) gets a black ring: v3.23.0 drawn in the glyph pass (below).
#define OPT_SUB_RGB  0x805820u  // 0xBBGGRR, 0x80 = full: R 0x20 G 0x58 B 0x80, a bright blue
#define OPT_SUB_HOT  0x601C06u  // under the cursor: deep blue
static int g_pending;  // (defined below: the target of the slide that runs)
static const DWORD g_fnLabel = 0x5239A0;
// v3.23.0 (user items 1 / 4 / 5: values and rows invisible): the text queue holds 800 glyph entries a frame (5570B0:
// `if ([0x1261BF4] < 800)`, else the glyph is dropped silently); v3.22's 8 queued ring copies a string filled it during
// the Filter row. The ring is now drawn by the glyph pass instead (subtitles.inc SubShadowC, the shadow quad D1 of each
// entry): OutlineShadow queues only the stock shadow string and records the queue range of the main string that the
// stock code queues right after it (same id, same glyph count).
#define OPT_RINGS 32  // v3.44 DF344: 24 -> 32 (Screen Effects: 11 rows = 11 labels + 10 values + the title + a description)
typedef struct { int n0, g, chk; } OptRing;  // the main string's entries n0 .. n0 + g - 1; chk 0 unchecked, 1 ok, -1 not ours
static OptRing g_optRing[OPT_RINGS];
static int g_optRingN, g_optRingEnd, g_optRingCopies = -1, g_optRingLast = -1;
static int __cdecl OutlineShadow(int bank, int id, int x, int y)  // was `call 50B580` (bank, id, x + 2, y + 2)
{
    volatile const int *cnt = (volatile const int *)0x1261BF4;
    const int n0 = *cnt;
    const int r = ((int(__cdecl *)(int, int, int, int))0x50B580)(bank, id, x, y);
    if (g_lpage == LP_FX || g_pending == LP_FX) {
        const int n1 = *cnt, g = n1 - n0;
        static int s_logged;
        if (n0 < g_optRingEnd) {  // the queue was flushed since the last record without a glyph pass clearing the list
            g_optRingN = g_optRingEnd = 0;
            g_optRingCopies = -1;
        }
        if (g > 0 && n1 + g <= 800 && g_optRingN < OPT_RINGS) {
            OptRing *o = &g_optRing[g_optRingN++];
            o->n0 = n1, o->g = g, o->chk = 0;
            g_optRingEnd = n1 + g;
        }
        if (!s_logged && g_optRingLast >= 0) {
            s_logged = 1;
            Log("options: Screen Effects text outlined in the glyph pass (%d copies a glyph; none queued)", g_optRingLast);
        }
    }
    return r;
}
// 0x525553 (was `call 5239A0`): the row label
static int OptTextRgb(int flash, int id, int y, int x, DWORD rgb);  // v3.33 MERGE: shared with RowValueC
static int __cdecl RowLabelC(int flash, int id, int y, int x, int idx)
{
    DWORD rgb;
    const int grey = CamRowGrey(g_drawSlot, g_drawRow) || PresRowGrey(g_drawSlot, g_drawRow) ||
                     RgRowGrey(g_drawSlot, g_drawRow);  // v3.47 RG347  // v3.33 CAM333: Option 6's Camera row while Playable Eileen's camera owns the view; v3.39 PR339: + a greyed Renderer row
    if (!grey && !SubmenuRow(g_drawSlot, g_drawRow)) return 0;  // stock
    rgb = grey ? (idx == 2 ? 0x303060u : 0x484848u) : idx == 2 ? OPT_SUB_HOT : OPT_SUB_RGB;  // v3.33 CAM333: grey (a dull red under the cursor)
    return OptTextRgb(flash, id, y, x, rgb);
}
// v3.33 MERGE: 0x525802 (was `call 5239A0`, the same registers as the label call): a row's value. Greyed: Fly (No
// Clip's own value id) while no over-the-shoulder camera is on (NcValueGreyed, W3); every other value: stock.
static int __cdecl RowValueC(int flash, int id, int y, int x, int idx)
{
    if (!NcValueIdGreyed(id) && !HbValueIdGreyed(id) && !PresValueIdGreyed(id) && !RgValueIdGreyed(id)) return 0;  // v3.47 RG347: + Regeneration  // stock (v3.37 MN337: + Curse Meter's Player / Both; v3.39 PR339: + a greyed Renderer row's value)
    return OptTextRgb(flash, id, y, x, idx == 2 ? 0x303060u : 0x484848u);
}
// The label / value text in one colour (as 5239A0, with 556A90's rgb in place of a palette index); returns 1.
static int OptTextRgb(int flash, int id, int y, int x, DWORD rgb)
{
    if (flash) {  // v3.23.0 (user item 6): the cursor's arrival flash, as 5239A0's flash path (0x5239B1) but in our blue:
        int k;    // alpha 0x30, the shadow + the label at x - flash and x + flash, alpha back to 0x80 (556A90 keeps the alpha)
        ((Fn1)0x556AE0)(0x30);
        for (k = -1; k <= 1; k += 2) {
            ((Fn1)0x556A50)(8);
            ((int(__cdecl *)(int, int, int, int))0x50B580)(0, id, x + k * flash + 2, y + 2);
            ((Fn1)0x556A90)((int)rgb);
            ((int(__cdecl *)(int, int, int, int))0x50B580)(0, id, x + k * flash, y);
        }
        ((Fn1)0x556AE0)(0x80);
        return 1;
    }
    ((Fn1)0x556A50)(8);
    ((int(__cdecl *)(int, int, int, int))0x50B580)(0, id, x + 2, y + 2);
    ((Fn1)0x556A90)((int)rgb);
    ((int(__cdecl *)(int, int, int, int))0x50B580)(0, id, x, y);
    return 1;
}
static __declspec(naked) void hkRowLabel(void)
{
    __asm {
        pushad
        mov ecx, [esp + 28h]  // the palette index ([esp+8] at entry, above the 8 saved registers)
        mov edx, [esp + 24h]  // x
        push ecx
        push edx
        push ebx              // y
        push esi              // the label id
        push eax              // the flash count
        call RowLabelC
        add esp, 20
        mov [esp + 1Ch], eax
        popad
        test eax, eax
        jnz done
        jmp dword ptr [g_fnLabel]  // stock: 5239A0 with the caller's stack and registers
    done:
        ret
    }
}
static __declspec(naked) void hkRowValue(void)  // v3.33 MERGE: 0x525802, as hkRowLabel
{
    __asm {
        pushad
        mov ecx, [esp + 28h]  // the palette index
        mov edx, [esp + 24h]  // x
        push ecx
        push edx
        push ebx              // y
        push esi              // the value id
        push eax              // the flash count
        call RowValueC
        add esp, 20
        mov [esp + 1Ch], eax
        popad
        test eax, eax
        jnz done
        jmp dword ptr [g_fnLabel]  // stock: 5239A0 with the caller's stack and registers
    done:
        ret
    }
}
// v3.19.0 C2b: 0x52570E (draw) and 0x525CE3 (mouse rects) stubs for long values
static int __cdecl ValueColC(void)
{
    int w;
    if ((unsigned)g_drawSlot >= 3) return 0;
    w = LongValW(g_slotPage[g_drawSlot], g_drawRow);
    return w ? LongMove(w) : 0;
}
static int __cdecl MouseColC(int row)
{
    const int lw = LongValW(g_lpage, row);
    if (!lw) return 0;
    g_mouseAdj = (lw - 100 + 6) - LongMove(lw);  // the right arrow's rect where it is drawn
    return LongMove(lw);
}
static const DWORD g_back525713 = 0x525713, g_to5257EA = 0x5257EA, g_back525CEA = 0x525CEA, g_to525D9D = 0x525D9D;
static __declspec(naked) void hkValueCol(void)  // 52570E: was `mov al,[1091ADCh]` (jmp here: esp is the function's)
{
    __asm {
        pushad
        call ValueColC
        mov [esp + 28], eax      // into the saved eax
        popad
        test eax, eax
        jnz moved
        mov al, byte ptr ds:[1091ADCh]
        jmp dword ptr [g_back525713]
    moved:
        sub ebp, eax
        mov [esp + 38h], ebp
        jmp dword ptr [g_to5257EA]
    }
}
static __declspec(naked) void hkMouseCol(void)  // 525CE3: was `cmp byte ptr [1091ADCh],2` (7 bytes)
{
    __asm {
        pushad
        push dword ptr [ebx + 30h]  // the cursor row
        call MouseColC
        add esp, 4
        mov [esp + 28], eax
        popad
        test eax, eax
        jnz moved
        cmp byte ptr ds:[1091ADCh], 2
        jmp dword ptr [g_back525CEA]
    moved:
        sub dword ptr [esp + 6Ch], eax  // the left arrow follows the column
        mov eax, dword ptr [g_mouseAdj]
        add dword ptr [esp + 24h], eax  // the right arrow's rect where it is drawn
        jmp dword ptr [g_to525D9D]
    }
}
static int g_pending = -1;  // the target of the slide that runs (declared above, v3.22.0)
static int g_pagesOn;

static void SetLogicalPage(int lp)  // the one place both identities change (review OPT-PAGE-003)
{
    g_lpage = lp;
    g_stockView = lp < 3 ? lp : 7;
}

// Fills physical slot `slot` with logical page lp: the slot's table pointers / count / Exit row / title, then its
// widget the way 523D70 builds it. The widget's rect nodes come from the game heap (5525D0 -> 551E90) and the Options
// exit frees them with 5521E0, so the old ones are freed first (5521E0) and the widget initialised again (5521B0).
static int SlotRowStep(int n) { return n <= 8 ? 32 : n == 9 ? 28 : n == 10 ? 26 : n == 11 ? 24 : 22; }  // v3.32 DESC332: shared with descfit.inc  // v3.33 CAM333: 11 rows 24, 12-13 rows 22 (the Camera page: 13; the last row ends at y 390, the description below it)
static void LoadSlot(int slot, int lp)
{
    BYTE *const w = (BYTE *)(ULONG_PTR)(0x1091A20 + 0x38 * slot);
    DWORD *const t = g_lpTable[lp];
    const int n = g_lpCount[lp];
    const int top = 96, step = SlotRowStep(n);  // v3.32 DESC332
    int i, cur;
    *(DWORD *)(ULONG_PTR)(0x655058 + 4 * slot) = (DWORD)(ULONG_PTR)t;
    *(DWORD *)(ULONG_PTR)(0x655074 + 4 * slot) = (DWORD)(ULONG_PTR)t;
    *(DWORD *)(ULONG_PTR)(0x655080 + 4 * slot) = (DWORD)(ULONG_PTR)t;
    *(int *)(ULONG_PTR)(0x655040 + 4 * slot) = n;
    *(int *)(ULONG_PTR)(0x655064 + 4 * slot) = n - 1;
    *(DWORD *)(ULONG_PTR)(0x65504C + 4 * slot) = g_lpTitle[lp];
    g_slotPage[slot] = lp;
    ((void(__cdecl *)(void *))0x5521E0)(w);
    ((void(__cdecl *)(void *))0x5521B0)(w);
    *(int *)(w + 0x2C) = n;
    *(int *)(w + 0x30) = 0;
    *(DWORD *)(w + 0x34) = g_lpTitle[lp];
    for (i = 0; i < n; ++i) {
        RowRect r;
        r.x = 40;
        r.y = top + step * i;
        r.w = 220;
        r.h = 24;
        r.a = r.b = 0;
        ((int(__cdecl *)(void *, RowRect))0x5525D0)(w, r);
        t[i * 7 + 2] = 0;  // the per-row runtime value 523D70 clears
    }
    cur = g_lcursor[lp];
    if (cur < 0 || cur >= n) cur = 0;
    ((void(__cdecl *)(void *, int))0x552490)(w, (short)cur);
    // v3.37 MN337 (the user: "the highlight always defaults to the top of the page before going to where it remembers"):
    // OptionsDrawPage 0x525990 highlights the row OptionsPage +0x30 holds, which 525AA0 copies from the widget only on the
    // active page's input frames (after SlideCommit / the fade in): set it with the widget's cursor, and the per-slot
    // cursor 0x1092238 (OptionsPageSlide 0x526210's mode 2 reads it) as SubScreen / Mode2Slide do, so a page switch, a
    // folder opening or Back never draws row 0 for a frame
    *(int *)(w + 0x30) = cur;
    *(int *)(ULONG_PTR)(0x1092238 + 4 * slot) = cur;
    if (cur && ((short(__cdecl *)(void *))0x552470)(w) != (short)cur) {
        static volatile LONG s_q;
        if (LogQuota(&s_q, 5)) LogAnomaly("options", "page %d (slot %d): the cursor %d did not take (the widget says %d)", lp, slot, cur,
                                          (int)((short(__cdecl *)(void *))0x552470)(w));
    }
}

// 523E4B (`call 523D70`, eax = menu object): every opening of Options starts on Option 1 with stock pages in all slots.
static void __cdecl ResetPages(void)
{
    int p;
    for (p = 0; p < 3; ++p) {
        *(DWORD *)(ULONG_PTR)(0x655058 + 4 * p) = (DWORD)(ULONG_PTR)g_lpTable[p];
        *(DWORD *)(ULONG_PTR)(0x655074 + 4 * p) = (DWORD)(ULONG_PTR)g_lpTable[p];
        *(DWORD *)(ULONG_PTR)(0x655080 + 4 * p) = (DWORD)(ULONG_PTR)g_lpTable[p];
        *(int *)(ULONG_PTR)(0x655040 + 4 * p) = g_lpCount[p];
        *(int *)(ULONG_PTR)(0x655064 + 4 * p) = g_lpCount[p] - 1;
        *(DWORD *)(ULONG_PTR)(0x65504C + 4 * p) = g_lpTitle[p];
        g_slotPage[p] = p;
    }
    for (p = 0; p < LP_MAX; ++p) g_lcursor[p] = 0;
    g_pending = -1;
    g_subReq = 0;
    InterlockedExchange(&g_fxStamp, -1);                     // v3.15.0 (FX-003): no stale preview stamp
    SetLogicalPage(0);
    g_snapFov = g_fovSel;  // v3.5.1: the ini-kept settings as Options opens
    g_snapFovFp = g_fovFpSel;
    g_snapFps = g_fpsSel;
    g_snapShine = g_sparkleOn != 0;
    for (p = 0; p < EM_N; ++p) g_snapEm[p] = g_emOn[p];  // v3.7.0
    g_snapSbOn = g_sbOn;  // v3.8.0
    g_snapSbBank = g_sbBank;
    PresSnapshot();  // v3.39 PR339: Renderer (dgVoodoo.conf read now) / Opening (was the v3.12.0 graphics rows)
    for (p = 0; p < SUB_N; ++p) g_snapSub[p] = g_sub[p];     // v3.13.0
    g_snapVib = g_vibLevel;                                  // v3.14.0
    for (p = 0; p < FX_N; ++p) g_snapFx[p] = g_fx[p];        // v3.15.0
    GpSnapshot();                                            // v3.25.0
    CamSnapshot();  // v3.33 CAM333
    ShSnapshot();  // v3.40 SH340
    PpSnapshot();  // v3.43 PP343
    DofSnapshot();  // v3.43 DF343
    FnSnapshot();  // v3.44 FN344
    g_snapOn = 1;
}

// v3.5.1: "No" at "Do you want to change the options?" (0x526B6F copies the backup back, then `call 524320` at
// 0x526B8D applies it): the ini-kept settings go back to their snapshot the way their rows set them.
static void __cdecl DiscardOwn(void)
{
    if (!g_snapOn) return;
    if (g_fovSel != g_snapFov) SetFov(g_snapFov, 1, 0);
    if (g_fovFpSel != g_snapFovFp) SetFov(g_snapFovFp, 1, 1);
    if (g_fpsSel != g_snapFps && g_snapFps >= 0 && g_snapFps < FPS_N) {
        InterlockedExchange(&g_fpsSel, g_snapFps);
        if (!g_iniFallback) WritePrivateProfileStringA("Window", "FrameRate", kFpsName[g_snapFps], g_iniPath);
        FpsUserChanged(kFps[g_snapFps]);
    }
    if ((g_sparkleOn != 0) != (g_snapShine != 0)) SparkleUserChanged(g_snapShine);
    {
        int k;
        for (k = 0; k < EM_N; ++k)
            if (g_emOn[k] != g_snapEm[k]) EmSet(k, g_snapEm[k]);  // v3.7.0
        if (g_sbOn != g_snapSbOn) SbSetOn(g_snapSbOn);           // v3.8.0
        if (g_sbBank != g_snapSbBank) SbSetBank(g_snapSbBank);
        PresDiscard();  // v3.39 PR339: Renderer / Opening, never applied (was the v3.12.0 graphics rows)
        for (k = 0; k < SUB_N; ++k)  // v3.13.0
            if (g_sub[k] != g_snapSub[k]) SubSet(k, g_snapSub[k]);
        if (g_vibLevel != g_snapVib) VibSetLevel(g_snapVib);  // v3.14.0 (cfg+0x0B comes back with the stock block)
        for (k = 0; k < FX_N; ++k)  // v3.15.0
            if (g_fx[k] != g_snapFx[k]) FxSet(k, g_snapFx[k]);
        GpDiscard();  // v3.25.0
        CamDiscard();  // v3.33 CAM333
        ShDiscard();  // v3.40 SH340
        PpDiscard();  // v3.43 PP343
        DofDiscard();  // v3.43 DF343
        FnDiscard();  // v3.44 FN344
    }
    Log("options: discarded; Option 4 / 5 settings back to how Options opened");
}
static __declspec(naked) void hkDiscard(void)  // 526B8D: call 524320 (apply the restored block)
{
    __asm {
        mov dword ptr [g_inDiscard], 1  // v3.12.0: SetResIndex inside 524320 must not apply the graphics rows
        mov eax, 524320h
        call eax
        mov dword ptr [g_inDiscard], 0
        call DiscardOwn
        ret
    }
}
static __declspec(naked) void hkInitPages(void)
{
    __asm {
        push eax
        call ResetPages
        pop eax
        push 523D70h             // tail call: 523D70 returns to 523E50
        ret
    }
}

// 525B7A: a page arrow or pad bit 1000h / 4000h. Stock: target = FC -/+ 1 wrapping over 0..2, [obj+104] = +1 / -1.
static void __cdecl PageSwitch(BYTE *obj, int next)
{
    const int from = *(int *)(obj + 0xFC);
    const int lp = g_lpage == LP_SUB || g_lpage == LP_FX ? 2 :
                   g_lpage >= LP_N && g_lpage < LP_MAX && g_lpParent[g_lpage] >= 0 ? g_lpParent[g_lpage] :  // v3.25.0
                   next ? (g_lpage + 1) % LP_N : (g_lpage + LP_N - 1) % LP_N,
              slot = lp % 3;  // v3.13.0; v3.15.0: the Screen Effects page too
    if ((unsigned)from < 3) g_lcursor[g_lpage] = (short)((int(__cdecl *)(void *))0x552470)(obj + 0x38 * from);
    if (g_lpage == LP_FX) g_lcursor[2] = 2;  // v3.15.0: back on Noise Effect
    if (g_lpage >= LP_N && g_lpage < LP_MAX && g_lpParent[g_lpage] >= 0) g_lcursor[g_lpParent[g_lpage]] = g_lpParentRow[g_lpage];
    LoadSlot(slot, lp);
    *(int *)(obj + 0x100) = slot;
    *(int *)(obj + 0x104) = next ? -1 : 1;
    g_pending = lp;
}
// v3.16.0 (RE_fade 3): Cancel or a page key on a hidden page = Back with the fade (as Button Config). 1 = asked for (the
// caller makes 525AA0 return 5: row code 4 -> fade out -> SubScreen -> fade in on Option 3).
static int __cdecl HiddenBack(void)
{
    if (!g_subScreenOn) return 0;
    if (g_lpage >= LP_N && g_lpage < LP_MAX && g_lpParent[g_lpage] >= 0) InterlockedExchange(&g_subReq, REQ_BACK);  // v3.25.0
    else if (g_lpage != LP_SUB && g_lpage != LP_FX) return 0;
    else InterlockedExchange(&g_subReq, g_lpage == LP_SUB ? 2 : 4);
    ((Fn1)0x56CC40)(0x9C42);  // the stock cancel / confirm sound (0x5261CB)
    return 1;
}

// 0x526CD2 (`call 522FE0`, Options state 5, reached only by a row return 4: dead in GOG v7, RE_fade 2a): with our
// request pending the screen is black (state 5 draws nothing, the global fade stays at 0x80) and no Options code is
// live, so the target page is loaded here and 1 returned: the stock code goes back to state 2, whose main loop fades
// in on slot FC. Each hidden page keeps its own slot (Subtitle Style 0, Screen Effects 1, Option 3 in 2), so the
// invariant of PageSwitch ("the target slot is never on screen") still holds afterwards.
static int __cdecl SubScreen(BYTE *cfg)
{
    BYTE *const obj = cfg - 0xB0;  // cfg = 0x1091AD0, the menu object 0x1091A20
    const LONG req = InterlockedExchange(&g_subReq, 0);
    const int from = *(int *)(obj + 0xFC);
    int lp, slot;
    if (!req) return ((int(__cdecl *)(BYTE *))0x522FE0)(cfg);
    if (req == 1 && g_lpage == 2) lp = LP_SUB;
    else if (req == 3 && g_lpage == 2) lp = LP_FX;
    else if ((req == 2 && g_lpage == LP_SUB) || (req == 4 && g_lpage == LP_FX)) lp = 2;
    else if ((req & REQ_OPEN) && (req & 0xFF) < LP_MAX && g_lpParent[req & 0xFF] == g_lpage) lp = req & 0xFF;  // v3.25.0
    else if (req == REQ_BACK && g_lpage >= LP_N && g_lpage < LP_MAX && g_lpParent[g_lpage] >= 0) lp = g_lpParent[g_lpage];
    else {  // a stale request: fade back in on the page that is there
        Log("options: page request %ld on page %d ignored", req, g_lpage);
        return 1;
    }
    if ((unsigned)from < 3) g_lcursor[g_lpage] = (short)((int(__cdecl *)(void *))0x552470)(obj + 0x38 * from);
    g_lcursor[lp] = lp == 2 ? (req == 2 ? 1 : 2) : 0;  // back on Subtitles / Noise Effect; a hidden page's first row
    if (req == REQ_BACK) g_lcursor[lp] = g_lpParentRow[g_lpage];  // v3.25.0: back on the row that opened the page
    slot = lp % 3;
    LoadSlot(slot, lp);
    *(int *)(obj + 0xFC) = slot;
    *(int *)(obj + 0x100) = slot;
    *(int *)(obj + 0xF8) = 0;   // no slide
    *(int *)(obj + 0x118) = 0;
    *(int *)(ULONG_PTR)(0x1092238 + 4 * slot) = g_lcursor[lp];  // the per-slot cursor the stock mode-2 code sets (0x5262BA)
    g_pending = -1;
    SetLogicalPage(lp);
    return 1;
}

// 0x5261B4 (`mov eax,[ebp+FCh]`, 525AA0's Cancel: cursor to Exit, sound, return 1 = the Options exit). On a hidden page:
// Back with the fade (return 5 through the common tail 0x5261DC).
static const DWORD g_backCancel = 0x5261BA, g_backCancelHidden = 0x5261DC;
static __declspec(naked) void hkCancel(void)  // ebp menu object, ebx widget, edi 1; [esp+10h] = 525AA0's result
{
    __asm {
        pushad
        call HiddenBack
        mov [esp + 1Ch], eax     // into the saved eax
        popad
        test eax, eax
        jnz hidden
        mov eax, [ebp + 0FCh]    // the 6 stolen bytes
        jmp dword ptr [g_backCancel]
    hidden:
        mov dword ptr [esp + 10h], 5
        jmp dword ptr [g_backCancelHidden]
    }
}

static const DWORD g_backSwitch = 0x525BF7;  // mov [esp+10h],ecx / [ebp+118h] = 0 / call 51E6C0 / return ecx
static __declspec(naked) void hkPageSwitch(void)  // ebp menu object, esi pad bits, edi 1
{
    __asm {
        call HiddenBack          // v3.16.0: on a hidden page a page key is Back with the fade (eax / ecx / edx are free)
        test eax, eax
        jz slide
        mov ecx, 5               // 525AA0 returns 5: row code 4, the sub-screen exit
        jmp dword ptr [g_backSwitch]
    slide:
        mov [ebp + 0F8h], edi    // stock: slide mode 1
        mov eax, 405600h         // the right arrow clicked? (through Refined's NextPage hook)
        call eax
        test eax, eax
        jnz next
        test esi, 4000h
        jnz next
        push 0
        jmp go
    next:
        push 1
    go:
        push ebp
        call PageSwitch
        add esp, 8
        mov ecx, 2               // the action: page change
        jmp dword ptr [g_backSwitch]
    }
}

// 526439 (`mov edx,[esi+100h]` before FC = edx): the slide ends.
static void __cdecl SlideCommit(const BYTE *obj)
{
    const int slot = *(const int *)(obj + 0x100);
    if (g_pending >= 0 && g_pending % 3 == slot) SetLogicalPage(g_pending);
    else SetLogicalPage((unsigned)slot < 3 ? slot : 0);  // mode 2 (stock page of that slot) or a slide we did not start
    g_pending = -1;
}
static const DWORD g_backCommit = 0x52643F;
static __declspec(naked) void hkSlideCommit(void)
{
    __asm {
        push eax
        push ecx
        push esi
        call SlideCommit
        add esp, 4
        pop ecx
        pop eax
        mov edx, [esi + 100h]
        jmp dword ptr [g_backCommit]
    }
}

// 5262B2: slide mode 2 ([obj+F8] == 2; 52627B picks slot 1 from slot 0, else slot 0). No row in GOG v7 starts it; if
// one ever does, the target slot shows its stock page and the commit makes that page current (review OPT-PAGE-002).
static void __cdecl Mode2Slide(int slot, BYTE *obj)
{
    static int logged;
    const LONG req = InterlockedExchange(&g_subReq, 0);
    if (req == 1 || req == 2) {  // v3.13.0: the Subtitle Style page opens (Option 3's Subtitles row) or closes (Back)
        const int to = req == 1 ? 0 : 2, lp = req == 1 ? LP_SUB : 2;
        *(int *)(obj + 0x100) = to;
        *(int *)(obj + 0x104) = req == 1 ? 1 : -1;
        g_lcursor[lp] = req == 1 ? 0 : 1;           // the page's first row / back on Subtitles
        *(int *)(ULONG_PTR)(0x1092238 + 4 * to) = g_lcursor[lp];  // the cursor the stock mode-2 code sets (0x5262BA)
        LoadSlot(to, lp);
        g_pending = lp;
        return;
    }
    if (req == 3 || req == 4) {  // v3.15.0: the Screen Effects page opens (Option 3's Noise Effect row) or closes (Back)
        const int lp = req == 3 ? LP_FX : 2, to = lp % 3;  // slot 1 (off screen: Option 3 is slot 2) / back to slot 2
        *(int *)(obj + 0x100) = to;
        *(int *)(obj + 0x104) = req == 3 ? 1 : -1;
        g_lcursor[lp] = req == 3 ? 0 : 2;           // the page's first row / back on Noise Effect
        *(int *)(ULONG_PTR)(0x1092238 + 4 * to) = g_lcursor[lp];  // the cursor the stock mode-2 code sets (0x5262BA)
        LoadSlot(to, lp);
        g_pending = lp;
        return;
    }
    if ((unsigned)slot >= 3) return;
    if (!logged) {
        logged = 1;
        Log("options: page slide mode 2 seen (to slot %d): loading its stock page", slot);
    }
    LoadSlot(slot, slot);
    g_pending = slot;
}
static const DWORD g_backMode2 = 0x5262B8;
static __declspec(naked) void hkMode2(void)
{
    __asm {
        pushad
        push esi
        push dword ptr [esi + 100h]
        call Mode2Slide
        add esp, 8
        popad
        mov eax, [esi + 100h]
        jmp dword ptr [g_backMode2]
    }
}

// v1.9 (audit INT-1): when the device reset fails (a size the GPU can't make, or a lost device), the stock code puts the
// old size back and the main tick tries again every frame. A repeat of the same pick with [0x610DD8] unchanged since the
// previous pick and no re-apply asked for (ForceReapply) is such a retry: it is logged once and doesn't reshape the window; after 10 the last size that worked (or
// 1024x768) is picked instead. A pick counts as working once [0x610DD8] holds its entry (0x4149D7, after the reset).
static int g_prevA = -1, g_prevIdx = -1, g_goodA = -1, g_goodIdx = -1, g_retries, g_prevDD8 = 0x7FFFFFFF;
static LONG g_prevGen = -1;
static void __cdecl PickSize(int idx)
{
    const LONG gen = g_reapplyGen;
    const int dd8 = *(int *)0x610DD8;
    int a = g_applied >= 0 ? g_applied : (idx >= 2 ? ASP_169 : ASP_43), same;
    if ((unsigned)idx > 5) idx = 1;
    if (g_prevIdx >= 0 && dd8 == g_prevIdx) {
        g_goodA = g_prevA;
        g_goodIdx = g_prevIdx;
        g_retries = 0;
    }
    same = a == g_prevA && idx == g_prevIdx && dd8 == g_prevDD8 && gen == g_prevGen;
    g_retries = same ? g_retries + 1 : 0;
    if (g_retries >= 10) {
        const int ga = g_goodA >= 0 ? g_goodA : ASP_43, gi = g_goodA >= 0 ? g_goodIdx : 1;
        if (ga != a || gi != idx) {
            Log("options: the device could not be reset to %dx%d (%d tries); back to %dx%d", kSizes[a][idx][0],
                kSizes[a][idx][1], g_retries, kSizes[ga][gi][0], kSizes[ga][gi][1]);
            a = ga;
            idx = gi;
            *(int *)0x610DD4 = idx;
            CFG[0x20] = (BYTE)idx;
            CFG[0x21] = (BYTE)(a + 1);
            g_applied = a;
            same = 0;
            g_retries = 0;
        }  // else nothing to fall back to: the stock retries go on, logged once
    }
    *(int *)0x5D524C = kSizes[a][idx][0];
    *(int *)0x5D5250 = kSizes[a][idx][1];
    if (same) {
        if (g_retries == 1) Log("options: the device reset to %dx%d failed; the game tries again", kSizes[a][idx][0], kSizes[a][idx][1]);
    } else {
        Log("options: Resolution entry %d, %s: game size %dx%d", idx, kAspName[a], kSizes[a][idx][0], kSizes[a][idx][1]);
        InterlockedExchange(&g_picW, kSizes[a][idx][0]);
        InterlockedExchange(&g_picH, kSizes[a][idx][1]);
        if (g_reshapeEvt) SetEvent(g_reshapeEvt);
    }
    g_prevA = a;
    g_prevIdx = idx;
    g_prevDD8 = dd8;
    g_prevGen = gen;
}

static __declspec(naked) void hkSizePick(void)  // 414917, ecx = entry (clamped to 0..5 by the stock code)
{
    __asm {
        push ecx
        call PickSize
        add esp, 4
        jmp dword ptr [g_backSize]   // 4149A9: [6E7DA4] = 1, device reset 402CD0, old size back if it fails
    }
}

// Camera (55BD10, at start-up, world load and after every device reset): cam+48 aspect and cam+44 zoom. Stock 4:3
// = 0.78125 / 1.0; scaled by (4/3) / (W/H) for any other shape: the same vertical view, wider sideways (Hor+).
static float GameShape(void)
{
    int w = *(int *)0x5D524C, h = *(int *)0x5D5250;
    return w > 0 && h > 0 ? (4.0f / 3.0f) * (float)h / (float)w : 1.0f;
}
static float __cdecl CamAspect(void) { return 0.78125f * GameShape(); }
static DWORD __cdecl CamZoomBits(void)
{
    float z = GameShape();
    DWORD b;
    memcpy(&b, &z, 4);
    return b;
}

// The game reads the mouse as GetCursorPos - client origin (407739) in GAME pixels; with FitWrapper the wrapper only
// removes the client origin (its scale is 1). With the window at another size than the game, both ways are scaled here.
// (Without FitWrapper the wrapper scales by display / game size itself, assuming the client is the desktop size.)
static HWND g_hwnd;  // the game window (set by Attach, below)
static void MapCursor(POINT *p, int toGame)
{
    RECT r;
    POINT o = { 0, 0 };
    int gw = *(int *)0x5D524C, gh = *(int *)0x5D5250;
    if (g_dm == DM_BORDERLESS) return;  // v1.5: display=desktop, the wrapper scales and offsets the cursor itself
    if (!g_hwnd || !GetClientRect(g_hwnd, &r) || r.right <= 0 || r.bottom <= 0 || gw <= 0 || gh <= 0) return;
    if (r.right == gw && r.bottom == gh) return;
    ClientToScreen(g_hwnd, &o);
    if (toGame) {
        p->x = o.x + MulDiv(p->x - o.x, gw, r.right);
        p->y = o.y + MulDiv(p->y - o.y, gh, r.bottom);
    } else {
        p->x = o.x + MulDiv(p->x - o.x, r.right, gw);
        p->y = o.y + MulDiv(p->y - o.y, r.bottom, gh);
    }
}

static BOOL WINAPI GameGetCursorPos(POINT *p)
{
    BOOL ok = (*(BOOL(WINAPI **)(POINT *))0x5B7218)(p);  // whatever the IAT holds (the wrapper's hook or USER32)
    if (ok && p) MapCursor(p, 1);
    return ok;
}

static BOOL WINAPI GameSetCursorPos(int x, int y)
{
    POINT p;
    p.x = x;
    p.y = y;
    MapCursor(&p, 0);
    return (*(BOOL(WINAPI **)(int, int))0x5B7220)(p.x, p.y);
}

typedef struct { DWORD va; BYTE stock[32]; int len; } Site;
// v3.33 LOG333 (W7): the start's byte checks and failed writes, for the health log's patch summary (loghealth.inc)
#define LH_SITE_KEEP 16
static int g_lhSiteOpen = 1, g_lhSiteBadN;
static LONG g_lhSiteChecks, g_lhPokeFails;
static DWORD g_lhSiteBad[LH_SITE_KEEP];
static int SiteOk(const Site *s)
{
    const int ok = !memcmp((const void *)(ULONG_PTR)s->va, s->stock, s->len);
    if (g_lhSiteOpen) {  // v3.33 LOG333 (W7)
        int i;
        ++g_lhSiteChecks;
        for (i = 0; !ok && i < g_lhSiteBadN && i < LH_SITE_KEEP && g_lhSiteBad[i] != s->va; ++i) {}
        if (!ok && (i >= g_lhSiteBadN || i >= LH_SITE_KEEP)) {
            if (g_lhSiteBadN < LH_SITE_KEEP) g_lhSiteBad[g_lhSiteBadN] = s->va;
            ++g_lhSiteBadN;
        }
    }
    return ok;
}
// v3.35 W0: a callee accepted as Playable Eileen's hook (camera.inc PeHookedCallee) is no failed byte check
static void LhSiteForgive(DWORD va)
{
    int i, n = g_lhSiteBadN < LH_SITE_KEEP ? g_lhSiteBadN : LH_SITE_KEEP;
    if (!g_lhSiteOpen) return;
    for (i = 0; i < n && g_lhSiteBad[i] != va; ++i) {}
    if (i >= n) return;
    for (; i + 1 < n; ++i) g_lhSiteBad[i] = g_lhSiteBad[i + 1];
    --g_lhSiteBadN;
}

// Writes len bytes at va (code or data), lists code bytes in SH4WM_Patches.
static int Poke(DWORD va, const void *bytes, int len)
{
    BYTE *p = (BYTE *)(ULONG_PTR)va;
    DWORD old;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) {
        InterlockedIncrement(&g_lhPokeFails);  // v3.33 LOG333 (W7)
        return 0;
    }
    memcpy(p, bytes, len);
    VirtualProtect(p, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    if (va < 0x5B7000) ListPatch(va, len);  // .text only; the cheat table restores code, not data
    return 1;
}

// E8 / E9 rel32 to `to`, padded with nops to len bytes.
static int PokeBranch(DWORD va, BYTE op, const void *to, int len)
{
    BYTE b[32];
    DWORD rel = (DWORD)(ULONG_PTR)to - (va + 5);
    memset(b, 0x90, sizeof b);
    b[0] = op;
    memcpy(b + 1, &rel, 4);
    return Poke(va, b, len);
}

// The 13 reads of [0x1091B1C] in the stock page special cases (opcode bytes checked, then the operand repointed).
static const struct { DWORD va; BYTE pre[2]; int prelen; } kPageReads[] = {
    { 0x525289, { 0xA1 }, 1 },       { 0x5252B9, { 0xA1 }, 1 },       { 0x5252E1, { 0xA1 }, 1 },
    { 0x5253A0, { 0x8B, 0x0D }, 2 }, { 0x5253F9, { 0x8B, 0x0D }, 2 }, { 0x525524, { 0x83, 0x3D }, 2 },
    { 0x5255BE, { 0x83, 0x3D }, 2 }, { 0x525791, { 0x83, 0x3D }, 2 }, { 0x525713, { 0x8B, 0x15 }, 2 },
    { 0x525848, { 0x8B, 0x35 }, 2 }, { 0x525CD9, { 0x8B, 0x3D }, 2 }, { 0x525F0C, { 0x83, 0x3D }, 2 },
    { 0x525F27, { 0x83, 0x3D }, 2 },
};

static void FillRow(DWORD *r, const void *input, const void *get, const DWORD *ids, int count, int label, const void *desc)
{
    r[0] = (DWORD)(ULONG_PTR)input;
    r[1] = (DWORD)(ULONG_PTR)get;
    r[2] = 0;
    r[3] = (DWORD)(ULONG_PTR)ids;
    r[4] = (DWORD)count;
    r[5] = (DWORD)label;
    r[6] = (DWORD)(ULONG_PTR)desc;
}

#include "camera.inc"  // v3.33 CAM333: the over-the-shoulder camera (before gameplay.inc: No Clip's Fly asks CamOtsOn)
#include "gameplay.inc"  // v3.25.0 Blood Colour values, Option 5 Player / Enemies pages and their cheats
#include "omni.inc"  // v3.46 OM346: Stick Movement (after gameplay.inc: its patch set and lock)  // v3.47 CT347: its row is on Option 2
#include "regen.inc"  // v3.47 RG347: Refined's own Regeneration (after gameplay.inc: the Player page, GpLock; before quicksave.inc: QsTick calls RgTick)
#include "randomizer.inc"  // v3.48 RZ348: the Randomizer (after gameplay.inc: its lock, rows, texts; before enemies.inc: EnmWrap calls it)

// Installs the page machinery once the Option 4 / 5 tables are filled. All or nothing: 0 if any site differs.
static int InstallPages(void)
{
    static const Site kSites[] = {
        { 0x525B7A, { 0x89, 0xBD, 0xF8, 0x00, 0x00, 0x00, 0xE8, 0x7B, 0xFA, 0xED, 0xFF, 0x85, 0xC0, 0xB9, 0x02, 0x00, 0x00,
                      0x00, 0x75, 0x44, 0xF7, 0xC6, 0x00, 0x40, 0x00, 0x00, 0x75, 0x3C }, 28 },
        { 0x525BF7, { 0x89, 0x4C, 0x24, 0x10, 0xC7, 0x85, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE8 }, 15 },
        { 0x526439, { 0x8B, 0x96, 0x00, 0x01, 0x00, 0x00, 0x89, 0x96, 0xFC, 0x00, 0x00, 0x00 }, 12 },
        { 0x5262B2, { 0x8B, 0x86, 0x00, 0x01, 0x00, 0x00, 0x33, 0xC9 }, 8 },
        { 0x523E4B, { 0xE8, 0x20, 0xFF, 0xFF, 0xFF }, 5 },
        { 0x526B8D, { 0xE8, 0x8E, 0xD7, 0xFF, 0xFF }, 5 },  // v3.5.1: call 524320 on discard
        { 0x655058, { 0xC0, 0x4C, 0x65, 0x00, 0x88, 0x4D, 0x65, 0x00, 0x50, 0x4E, 0x65, 0x00 }, 12 },
        { 0x655074, { 0xC0, 0x4C, 0x65, 0x00, 0x88, 0x4D, 0x65, 0x00, 0x50, 0x4E, 0x65, 0x00 }, 12 },
        { 0x655080, { 0xC0, 0x4C, 0x65, 0x00, 0x88, 0x4D, 0x65, 0x00, 0x50, 0x4E, 0x65, 0x00 }, 12 },
        { 0x655034, { 7, 0, 0, 0, 7, 0, 0, 0, 8, 0, 0, 0 }, 12 },
        { 0x655040, { 7, 0, 0, 0, 7, 0, 0, 0, 8, 0, 0, 0 }, 12 },
        { 0x65504C, { 0x6E, 0, 0, 0, 0x6F, 0, 0, 0, 0x70, 0, 0, 0 }, 12 },
        { 0x655064, { 6, 0, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0 }, 12 },
    };
    int i, bad = 0;
    DWORD ref;
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            Log("options pages: unexpected bytes at %08lX", kSites[i].va);
            ++bad;
        }
    for (i = 0; i < (int)(sizeof kPageReads / sizeof kPageReads[0]); ++i) {
        const BYTE *p = (const BYTE *)(ULONG_PTR)kPageReads[i].va;
        if (memcmp(p, kPageReads[i].pre, kPageReads[i].prelen) || *(const DWORD *)(p + kPageReads[i].prelen) != 0x1091B1C) {
            Log("options pages: unexpected page read at %08lX", kPageReads[i].va);
            ++bad;
        }
    }
    if (bad) {
        Log("options pages: %d site(s) differ from the GOG v7 exe (another mod?): no Options 4-5; their settings stay in the ini", bad);
        return 0;
    }
    for (i = 0; i < 3; ++i) {
        g_lpTable[i] = (DWORD *)(ULONG_PTR)*(const DWORD *)(ULONG_PTR)(0x655058 + 4 * i);
        g_lpCount[i] = *(const int *)(ULONG_PTR)(0x655040 + 4 * i);
        g_lpTitle[i] = *(const DWORD *)(ULONG_PTR)(0x65504C + 4 * i);
    }
    {   // v3.13.0: Option 3 becomes a copy of its stock table whose Subtitles row (row 1) also opens Subtitle Style.
        // Only when the stock table is what the RE saw (8 rows; row 1 = input 0x524AF0, label 0x2B, 2 values).
        // v3.15.0: and whose Noise Effect row (row 2 = input 0x524E40, getter 0x413CD0, label 0x2F, 2 values) also opens
        // Screen Effects; each row is replaced only if it is the stock one.
        const DWORD *s3 = g_lpTable[2];
        const int stock3 = s3 == (const DWORD *)0x654E50 && g_lpCount[2] == 8;
        const int sub = stock3 && s3[7 + 0] == 0x524AF0 && s3[7 + 5] == 0x2B && s3[7 + 4] == 2;
        const int fx = stock3 && g_fxOn && s3[14 + 0] == 0x524E40 && s3[14 + 1] == 0x413CD0 && s3[14 + 5] == 0x2F && s3[14 + 4] == 2;
        // v3.25.0: and whose Blood Colour row (row 3, re_blood.md 1) has 10 values when the colour hooks are in
        const int bld = stock3 && g_bldOn && s3[21 + 0] == 0x524DA0 && s3[21 + 1] == 0x413CC0 && s3[21 + 3] == 0x654C48 &&
                        s3[21 + 4] == 3 && s3[21 + 5] == 0x2C && s3[21 + 6] == 0x525490;
        // v3.40 SH340: and whose Realtime Shadow row (row 6 = input 0x524F20, getter 0x413CF0 (cfg+0x19), label 0x78, 2 values)
        // also opens the Shadows page
        const int shd = stock3 && ShRowWanted() && s3[42 + 0] == 0x524F20 && s3[42 + 1] == 0x413CF0 && s3[42 + 5] == 0x78 && s3[42 + 4] == 2;
        // v3.43 DF343: and whose Hide Icon row (row 5 = input 0x524830, getter 0x413C50 (cfg+0x0A), label 0x22, 2 values) also
        // opens the Depth of Field page
        const int dof = stock3 && DofRowWanted() && s3[35 + 0] == 0x524830 && s3[35 + 1] == 0x413C50 && s3[35 + 5] == 0x22 && s3[35 + 4] == 2;
        if (sub || fx || bld || shd || dof) {  // v3.43 DF343: + dof  // (the stock table has no terminator: the next table follows; the count says 8)  // v3.40 SH340: + shd
            memcpy(g_rowsOthers, s3, 8 * 7 * 4);
            g_rowsOthers[8 * 7] = 0;
            if (shd) ShRowInstall(g_rowsOthers + 42);  // v3.40 SH340: Realtime Shadow's Enter opens the Shadows page
            if (dof) DofRowInstall(g_rowsOthers + 35);  // v3.43 DF343: Hide Icon's Enter opens the Depth of Field page
            if (sub) {
                g_rowsOthers[7 + 0] = (DWORD)(ULONG_PTR)SubRowInput;
                g_rowsOthers[7 + 6] = (DWORD)(ULONG_PTR)SubRowDesc;
                g_subMenu = 1;
            }
            if (fx) {
                g_fxStockIn = g_rowsOthers[14 + 0];  // the stock input, kept from the copied row
                g_rowsOthers[14 + 0] = (DWORD)(ULONG_PTR)FxRowInput;
                g_rowsOthers[14 + 6] = (DWORD)(ULONG_PTR)FxRowDesc;
                g_fxMenu = 1;
            }
            if (bld) {  // the stock input (0x524DA0) wraps by the count; ours repairs an out-of-range byte first (REV-005)
                g_rowsOthers[21 + 0] = (DWORD)(ULONG_PTR)BldIn;
                g_rowsOthers[21 + 1] = (DWORD)(ULONG_PTR)BldGet;
                g_rowsOthers[21 + 3] = (DWORD)(ULONG_PTR)g_bldIds;
                g_rowsOthers[21 + 4] = BLD_N;
                g_rowsOthers[21 + 6] = (DWORD)(ULONG_PTR)BldDesc;
                g_bldRowOn = 1;  // (only takes effect when InstallPages succeeds; see below)
                Log("blood colour: Option 3's Blood Colour row has %d values", BLD_N);
            }
            g_lpTable[2] = g_rowsOthers;
        }
        if (!sub)
            Log("subtitles: Option 3's table is not the stock one (%p, %d rows): no Subtitle Style page", (const void *)s3, g_lpCount[2]);
        if (g_bldOn && !bld)
            Log("blood colour: Option 3's Blood Colour row is not the stock one: it stays Red / Green / Purple (the hooks keep those)");
        if (DofRowWanted() && !dof)  // v3.43 DF343
            Log("depth of field: Option 3's Hide Icon row is not the stock one (%p, %d rows): no Depth of Field page; "
                "[ScreenEffects] DepthOfField still applies", (const void *)s3, g_lpCount[2]);
        if (ShRowWanted() && !shd)  // v3.40 SH340
            Log("shadows: Option 3's Realtime Shadow row is not the stock one (%p, %d rows): no Shadows page; [Shadows] Edge still applies",
                (const void *)s3, g_lpCount[2]);
        if (g_fxOn && !fx)
            Log("screen effects: Option 3's Noise Effect row is not the stock one (%p, %d rows): no Screen Effects page; the "
                "[ScreenEffects] keys still apply", (const void *)s3, g_lpCount[2]);
    }
    {   // v3.14.0: Option 2 becomes a copy whose Vibration row (row 2) has Off / Low / Medium / High
        const DWORD *s2 = g_lpTable[1];
        if (g_vibOn && s2 == (const DWORD *)0x654D88 && g_lpCount[1] == 7 && s2[14 + 0] == 0x524980 && s2[14 + 1] == 0x413C60 &&
            s2[14 + 5] == 0x23 && s2[14 + 4] == 2) {
            memcpy(g_rowsControl, s2, 7 * 7 * 4);
            g_rowsControl[7 * 7] = 0;
            g_rowsControl[14 + 0] = (DWORD)(ULONG_PTR)VibIn;
            g_rowsControl[14 + 1] = (DWORD)(ULONG_PTR)VibGet;
            g_rowsControl[14 + 3] = (DWORD)(ULONG_PTR)g_vibIds;
            g_rowsControl[14 + 4] = 4;
            g_rowsControl[14 + 6] = (DWORD)(ULONG_PTR)VibDesc;
            g_lpTable[1] = g_rowsControl;
            g_vibMenu = 1;
        } else if (g_vibOn) {
            Log("vibration: Option 2's table is not the stock one (%p, %d rows): the row stays Off / On", (const void *)s2, g_lpCount[1]);
        }
    }
    SetLogicalPage(0);
    ref = (DWORD)(ULONG_PTR)&g_stockView;
    for (i = 0; i < (int)(sizeof kPageReads / sizeof kPageReads[0]); ++i)
        Poke(kPageReads[i].va + kPageReads[i].prelen, &ref, 4);
    PokeBranch(0x525B7A, 0xE9, hkPageSwitch, 6);
    PokeBranch(0x526439, 0xE9, hkSlideCommit, 6);
    PokeBranch(0x5262B2, 0xE9, hkMode2, 6);
    PokeBranch(0x523E4B, 0xE8, hkInitPages, 5);
    PokeBranch(0x526B8D, 0xE8, hkDiscard, 5);  // v3.5.1
    g_pagesOn = 1;
    if (g_subMenu || g_fxMenu || g_gpMenu || ShMenuOn() || DofMenuOn()) {  // v3.43 DF343: + the Depth of Field page  // v3.40 SH340: + the Shadows page  // v3.16.0 (RE_fade 3): the hidden pages fade in and out (both hooks or neither)
        static const Site kFade[] = {
            { 0x526CCD, { 0x68, 0xD0, 0x1A, 0x09, 0x01, 0xE8, 0x09, 0xC3, 0xFF, 0xFF, 0xEB, 0xE1 }, 12 },  // push cfg; call 522FE0; jmp 526CBA
            { 0x5261B4, { 0x8B, 0x85, 0xFC, 0x00, 0x00, 0x00, 0x33, 0xC9, 0x66, 0x8B, 0x0C, 0x85, 0x64, 0x50, 0x65, 0x00 }, 16 },
            { 0x5261D8, { 0x89, 0x7C, 0x24, 0x10, 0xE8, 0xDF, 0x84, 0xFF, 0xFF, 0x8B, 0x44, 0x24, 0x10 }, 13 },  // the common tail
            { 0x526182, { 0xC7, 0x44, 0x24, 0x10, 0x05, 0x00, 0x00, 0x00 }, 8 },   // row code 4 -> 525AA0 returns 5
            { 0x526B0D, { 0x6A, 0x05, 0xC7, 0x83, 0x08, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 }, 12 },  // page frame 4 -> fade out
            { 0x526C7B, { 0x6A, 0x05, 0xE8, 0x5E, 0x7A, 0xFF, 0xFF }, 7 },         // main loop 4 -> state 5
            { 0x522FE0, { 0x56, 0x33, 0xF6, 0xE8, 0xC8, 0xB6, 0xFF, 0xFF }, 8 },
        };
        static const BYTE kCall[5] = { 0xE8, 0x09, 0xC3, 0xFF, 0xFF };
        int ok = 1;
        for (i = 0; i < (int)(sizeof kFade / sizeof kFade[0]); ++i)
            if (!SiteOk(&kFade[i])) {
                Log("options: %08lX differs from the GOG v7 exe (another mod?): the hidden pages slide (no fade)", kFade[i].va);
                ok = 0;
                break;
            }
        if (ok && PokeBranch(0x526CD2, 0xE8, SubScreen, 5)) {
            if (PokeBranch(0x5261B4, 0xE9, hkCancel, 6)) {
                g_subScreenOn = 1;
                Log("options: the hidden pages (Subtitle Style, Screen Effects, Player, Enemies) fade in and out like Button Config "
                    "(00526CD2, Cancel 005261B4)");
            } else if (!Poke(0x526CD2, kCall, 5) || memcmp((const void *)0x526CD2, kCall, 5)) {
                Log("options: 00526CD2 could not be put back (it forwards to the stock call): the hidden pages slide");
            } else {
                Log("options: 005261B4 could not be written: the hidden pages slide (no fade)");
            }
        } else if (ok) {
            Log("options: 00526CD2 could not be written: the hidden pages slide (no fade)");
        }
    }
    return 1;
}

// ================================================================ v3.5.2: save titles name the world the save was made in
// Plan and review: staging/save_names/PLAN.md (claudex-loop, Codex APPROVED round 2), RE: staging/save_names/RE.md.
// The slot title is a code picked at save time: 0x56F760 (0x12 for clear data, else `call 4D03C0` at 0x56F76F, its
// only caller) returns T, mapped through the pair table 0x69DB60 to the code stored in the slot entry. 0x4D03C0 only
// asks how Henry last came back to Room 302 (no hole yet -> "Room 302"; came through a hole -> that hole's world;
// else 4D02E0 of the last DIRECT return [13D81E0] / [13D81E2]), so a save made in a world (the cheat table's
// Pause Menu+ "Save Game" runs the stock save task 0x4CCDC0) was titled after an older world, and every 2nd-half save
// "Return from Apartment World". In a world the game's own 0x4D02E0(stage, room, 2nd-half bit 0x4FC830) is asked
// with the CURRENT stage [0xFD5A90] / room [0xFD5A94] (absolute VAs); Room 302, the tunnel (0x0A), the final stage and
// any place that function has no title for keep the stock answer. T stays in the game's 0..0x11 (never 0x12).
static int __cdecl SaveTitleT(void)
{
    const int st = *(volatile int *)0xFD5A90, rm = *(volatile int *)0xFD5A94;
    const int stock = ((int(__cdecl *)(void))0x4D03C0)();
    int t = 0;
    switch (st) {
    case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 11:
        t = ((int(__cdecl *)(int, int, int))0x4D02E0)(st, rm, ((int(__cdecl *)(void))0x4FC830)());
        break;
    default:
        break;
    }
    if (t <= 0 || t > 0x11) t = stock;
    Log("saves: title for stage %d room %d: T %d (the game's own rule: %d)", st, rm, t, stock);
    return t;
}

static void ApplySaveTitles(void)
{
    static const Site kSite = { 0x56F76F, { 0xE8, 0x4C, 0x0C, 0xF6, 0xFF, 0x8B, 0x0D, 0x60, 0xDB, 0x69, 0x00 }, 11 };
    static const Site kFns[] = {
        { 0x4D03C0, { 0xE8, 0x4B, 0x17, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x03, 0x33, 0xC0, 0xC3 }, 12 },
        { 0x4D02E0, { 0x8B, 0x4C, 0x24, 0x04, 0x83, 0xC1, 0xFE, 0x33, 0xC0, 0x83, 0xF9, 0x09 }, 12 },
        { 0x4FC830, { 0x33, 0xC0, 0xA0, 0x2D, 0x81, 0x3D, 0x01, 0xC1, 0xE8, 0x02, 0x83, 0xE0, 0x01, 0xC3 }, 14 },
    };
    int i;
    if (!g_saveTitles) {
        Log("saves: WorldTitles=0: the game's own save titles");
        return;
    }
    if (!SiteOk(&kSite)) {
        Log("saves: 0056F76F is not the stock `call 004D03C0` (another mod?): the game's own save titles");
        return;
    }
    for (i = 0; i < (int)(sizeof kFns / sizeof kFns[0]); ++i)
        if (!SiteOk(&kFns[i])) {
            Log("saves: %08lX is not the stock function (another mod?): the game's own save titles", kFns[i].va);
            return;
        }
    if (PokeBranch(0x56F76F, 0xE8, SaveTitleT, 5))  // review SAVE-TITLE-001: report what really happened
        Log("saves: a save made in a world is titled after that world (call 0056F76F hooked)");
    else
        Log("saves: could not patch 0056F76F (memory protection): the game's own save titles");
}

// ================================================================ v1.4: 4:3 menus, text and HUD in wide shapes

// [Window] Menus43. The game maps every 2D canvas onto the whole buffer with vertex-shader constants set once per device
// reset by 0x404D80 (c2/c3: the 512x448 canvas, c94/c95: 640x480; research\projection\report.md section 4), so in a
// wide shape menus, text, HUD and movies stretch. Ported from ThirteenAG's WidescreenFixesPack SilentHill4.WidescreenFix
// (MIT; research\projection\work\thirteenag, mapped to GOG v7): an "interface" set with x squeezed by 1 / S
// (S = (W/H) / (4/3)) is the default, and the batch flush 0x564210 switches to the game's own ("native") set for the
// batches that are part of the world: perspective sprites (shader space 2), effects by their call site (darkness, decals,
// blood, halos, light grid, the 512x448 fade, document and menu backdrops), screen-copy textures and drawing into an
// off-screen target. Cinema bars are drawn native (full width). Peep holes (Room 302 wall hole, door scope, Water
// Prison) get black sides, the door scope's lens samples the middle of the picture, and the menu mouse follows the 4:3
// area. At 4:3 (S = 1) both sets are the same.
#include "views_sim.h"  // v3.41 VW341: BlackSides' pillar widths, the Borderless edge-cover rule (test\views_sim_test.c)
typedef struct { float c2[4], c3[4], c94[4], c95[4]; } Consts2D;
static Consts2D g_native, g_iface;
static int g_haveConsts, g_ifaceSet;
static float g_scale2d = 1.0f;  // S of the current game size
static void *g_backBuffer;      // the device's back buffer (identity only; no reference kept across resets)
static BYTE *g_tramp;           // trampolines for the inline hooks
static int g_trampUsed;

#define DEV (*(void ***)0x6E7D94)  // the game's IDirect3DDevice8 *
#define VT(dev, i) ((*(void ***)(dev))[i])
typedef HRESULT(STDMETHODCALLTYPE *VsConst_t)(void *, DWORD, void *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *GetSurf_t)(void *, void **);
typedef HRESULT(STDMETHODCALLTYPE *GetBB_t)(void *, UINT, int, void **);
typedef ULONG(STDMETHODCALLTYPE *Release_t)(void *);
typedef struct { LONG x1, y1, x2, y2; } Rect8;
typedef struct { DWORD x, y, w, h; float minZ, maxZ; } Viewport8;
typedef HRESULT(STDMETHODCALLTYPE *Clear_t)(void *, DWORD, const Rect8 *, DWORD, DWORD, float, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *Viewport_t)(void *, Viewport8 *);
enum { VT_RELEASE = 2, VT_GETBACKBUFFER = 16, VT_GETRT = 32, VT_CLEAR = 36, VT_SETVP = 40, VT_GETVP = 41, VT_SETVSC = 79, VT_GETVSC = 80 };

// v3.42 EG342: the half-pixel offsets of the uploaded canvases (views_sim.h VwAlignNdc; 0 with PixelAlign2D=0). g_native /
// g_iface stay the game's mapping (items.inc / screenfx.inc / depthfix.inc project through them); only the upload moves.
static float g_alignDx, g_alignDy;

static void Apply2D(const Consts2D *c)
{
    void *dev = DEV;
    float c3[4], c95[4];  // v3.42 EG342
    if (!dev) return;
    memcpy(c3, c->c3, sizeof c3);
    memcpy(c95, c->c95, sizeof c95);
    c3[0] += g_alignDx; c3[1] += g_alignDy;    // v3.42 EG342: canvas edges on pixel edges (half a pixel left / up)
    c95[0] += g_alignDx; c95[1] += g_alignDy;  // v3.42 EG342
    ((VsConst_t)VT(dev, VT_SETVSC))(dev, 2, (void *)c->c2, 1);
    ((VsConst_t)VT(dev, VT_SETVSC))(dev, 3, (void *)c3, 1);
    ((VsConst_t)VT(dev, VT_SETVSC))(dev, 94, (void *)c->c94, 1);
    ((VsConst_t)VT(dev, VT_SETVSC))(dev, 95, (void *)c95, 1);
    g_ifaceSet = c == &g_iface;
}

// 0x4052B9: was `call 404D80` (after every device create / reset): the game's constants, then the interface set.
static void __cdecl SetConstants2D(void)
{
    static int logged;
    void *dev, *bb = NULL;
    int w = *(int *)0x5D524C, h = *(int *)0x5D5250, ok = 1, i;
    ((void(__cdecl *)(void))0x404D80)();
    if ((dev = DEV) == NULL) return;
    for (i = 0; i < 4; ++i)
        ok &= SUCCEEDED(((VsConst_t)VT(dev, VT_GETVSC))(dev, i < 2 ? 2 + i : 92 + i, i == 0 ? g_native.c2 : i == 1 ? g_native.c3 : i == 2 ? g_native.c94 : g_native.c95, 1));
    if (!ok || g_native.c2[0] == 0.0f) {  // the wrapper keeps no copy: the game's formulas (404D80, sx / sy = 1)
        float sx = *(float *)0x5D5264, sy = *(float *)0x5D5268;
        Consts2D d = { { sx / 256.0f, -sy / 224.0f, 1.0f / 16777216.0f, 1.0f }, { -8.0f * sx, 9.142857f * sy, 0, 0 },
                       { 1.0f / 320.0f, -1.0f / 240.0f, 1.0f / 16777216.0f, 1.0f }, { -1.0f, 1.0f, 0, 0 } };
        g_native = d;
    }
    g_scale2d = w > 0 && h > 0 ? (3.0f * w) / (4.0f * h) : 1.0f;
    g_iface = g_native;
    if (g_scale2d > 1.001f) {
        g_iface.c2[0] = g_native.c2[0] / g_scale2d;
        g_iface.c3[0] = g_iface.c2[0] * -2048.0f;
        g_iface.c94[0] = g_native.c94[0] / g_scale2d;
        g_iface.c95[0] = g_native.c95[0] / g_scale2d;
    }
    if (SUCCEEDED(((GetBB_t)VT(dev, VT_GETBACKBUFFER))(dev, 0, 0, &bb)) && bb) {
        g_backBuffer = bb;
        ((Release_t)VT(bb, VT_RELEASE))(bb);
    } else
        g_backBuffer = NULL;
    g_haveConsts = 1;
    if (g_pixAlign)  // v3.42 EG342
        VwAlignNdc(w, h, &g_alignDx, &g_alignDy);
    else
        g_alignDx = g_alignDy = 0.0f;
    Apply2D(&g_iface);
    if (logged++ < 8)
        Log("menus 4:3: game size %dx%d, S %.3f, constants %s (c2.x %g, c94.x %g), back buffer %p", w, h, g_scale2d,
            ok ? "read back" : "from the game's formulas", g_native.c2[0], g_native.c94[0], g_backBuffer);
    {  // v3.42 EG342: the edge decision, once per size
        static int s_lw, s_lh, s_lines;
        if ((w != s_lw || h != s_lh) && s_lines < 8) {
            int left = 0, right = 0;
            const int pil = g_scale2d > 1.001f && VwPillars(w, h, &left, &right);
            s_lw = w;
            s_lh = h;
            ++s_lines;
            if (g_pixAlign)
                LogV("2D edges: %dx%d: the 2D canvases sit on pixel edges (half a pixel left / up: clip %+.6f, %+.6f), so "
                     "full-screen pictures, fills and the grid cover column 0 / row 0 with half a pixel to spare%s",
                     w, h, g_alignDx, g_alignDy, !pil ? "; no black sides (4:3 or narrower)" : "");
            else
                LogV("2D edges: %dx%d: [Window] PixelAlign2D=0: the game's own 2D mapping (column 0 / row 0 may stay lit "
                     "on some backends)%s", w, h, !pil ? "; no black sides (4:3 or narrower)" : "");
            if (pil)
                LogV("2D edges: %dx%d: black sides %d | %d px (pixel columns 0..%d and %d..%d)%s", w, h, left, right,
                     left - 1, w - right, w - 1, g_pixAlign ? ", drawn as quads 1 px past the screen edges after the Clear" : " (Clear only)");
        }
    }
}

// Export (winmm.def): the squeeze S of the game's 2D right now, 1 = none (4:3, Menus43=0 or no device yet). Overlays
// drawn in pixels (Playable Eileen's health plus, v0.87) map canvas x to centre + (x - 256) * width / (512 * S) with it,
// so they stay with the game's 2D in the 4:3 middle.
float __cdecl SH4WM_Scale2D(void)
{
    return g_haveConsts && g_scale2d > 1.001f ? g_scale2d : 1.0f;
}

static int DrawingOffscreen(void)
{
    void *dev = DEV, *rt = NULL;
    if (!dev || !g_backBuffer || FAILED(((GetSurf_t)VT(dev, VT_GETRT))(dev, &rt)) || !rt) return 0;
    ((Release_t)VT(rt, VT_RELEASE))(rt);
    return rt != g_backBuffer;
}

// Return addresses of the calls into the flush that draw world-anchored effects or full-screen backdrops (GOG v7)
static const DWORD kWorldSites[] = {
    0x42E83E,            // darkness object cap
    0x45EBD7,            // world decals
    0x47771C,            // blood stains
    0x42EE12,            // darkness ribbons
    0x546123, 0x5461AE,  // light halos (545AE0)
    0x546598,            // light halos (5462D0)
    0x433EA6,            // light grid
    0x56622D,            // 512x448 fade
    0x431DEB,            // document viewer black backdrop
    0x553286,            // the wobbling grid behind the Options and in-game menus
};
// ---------------------------------------------------------------- v3.2.4 WORLD EFFECTS (begin)
// User (2026-11-09): "Refinement Mod aspect ratio option set higher than 4:3 causes enemy wall decals like the ghost
// portals and leech shadows to hover off the walls. Floating in mid-air." These effects are projected on the CPU through
// the world camera (55BF50 = view of camera 12E3E30 x screen matrix of 12E40C0, GS units around 2048; used by 468560 and
// its helpers 468610 / 468850 / 4689C0 / 468A90 / 468B60 / 468C40, the ribbon builder 45EC00 and the particle drawers)
// and drawn through the flush in shader space 0, the 512x448 canvas. The camera is Hor+ (CamAspect / zoom from the real
// size), so the canvas already holds the wide view; the interface constants (c2.x / S) then pulled every vertex toward
// the middle by 1 / S while the 3D around it filled the screen: off the walls, worse the farther from the centre and
// the wider the shape (S 1.33 at 16:9, 2.37 at 32:9). ThirteenAG's list above has only the darkness, decal, blood, halo
// and light grid sites; the space-2 decals (45E8C0, 45F740, 460D90, 4610A0) were already native. GOG's own 16:9 sizes
// keep the native constants, so the stock game has no such offset; Menus43=0 does not have it either.
// Found by a scan of all 98 calls of the flush: the space-0 / space-1 calls whose vertices come from the world projection
// (each checked at start: `call 564210` at ret - 5, else that entry is dropped).
static DWORD g_worldSites324[] = {
    0x45F3FA,            // Tremer (leech, hil) wall shadow: subtractive (42h) ribbon 45F1E0 <- 45F480 <- 48A600 <- 48AC70
    0x45F1CA,            // type 01 (Water Prison actor) additive ribbon 45F000 <- 45F410 <- 475040
    0x460652, 0x4607AD,  // Wall Man (kabe) wall quads 460010 <- 49CB70 <- 49D190
    0x461862, 0x461BBC,  // Room 302 wall ghost 4614C0 <- 461C00 / 461F50 / 462170 <- 4868B0
    0x460B09,            // flame billboards 4607D0 <- 4696D0 <- 469870 (Jasper / Victim 04 47DEC0, type 16h 477CC0)
    0x460D6B,            // flame strips 460B20 <- 4693D0 <- 469870
    0x430E70, 0x4326A1, 0x434B04, 0x435C44, 0x436B58, 0x437EDE,  // particle effects (each projects with 55BF50)
    0x438E92, 0x439B6B, 0x43AD18, 0x43B73C, 0x43CD74,            // particle effects
    0x4337C2,            // effect 437D40's full-canvas flicker strips (untextured, 34 vertices: past the 6-vertex fill rule)
    0x445466,            // SMG muzzle flash 445160 (bone point through 55BF50)
    0x54DAC8, 0x54DEF6,  // event effect 54CC10 (54D780; 54DE60 <- 54DAF0)
};
#define WORLD324_N ((int)(sizeof g_worldSites324 / sizeof g_worldSites324[0]))

static void CheckWorldSites324(void)  // ApplyMenus43, before the flush hook
{
    int i, n = 0;
    for (i = 0; i < WORLD324_N; ++i) {
        const BYTE *p = (const BYTE *)(ULONG_PTR)(g_worldSites324[i] - 5);
        DWORD rel;
        memcpy(&rel, p + 1, 4);
        if (p[0] == 0xE8 && g_worldSites324[i] + rel == 0x564210) {
            ++n;
            continue;
        }
        Log("menus 4:3: unexpected bytes at %08lX (%02X %02X %02X %02X %02X): that world effect stays in the 4:3 middle",
            g_worldSites324[i] - 5, p[0], p[1], p[2], p[3], p[4]);
        g_worldSites324[i] = 0;  // a return address is never 0
    }
    Log("menus 4:3: world effects on the walls and in the air (Tremer shadows, Wall Men, wall ghost, flames, particles, "
        "muzzle flash) full width: %d of %d flush sites", n, WORLD324_N);
}
// ---------------------------------------------------------------- v3.2.4 WORLD EFFECTS (end)
// Screen-copy / render-target texture slots: the screen copy (4036E0) and the target table read by 403810
static void *const *const kScreenTex[] = {
    (void *const *)0x13DC3A0, (void *const *)0x6E8DC4, (void *const *)0x6E8DC8, (void *const *)0x6E8DF0,
    (void *const *)0x6E8DF4, (void *const *)0x6E8DF8, (void *const *)0x6E8DFC,
};
#define LENS_SITE 0x4F96B9

typedef void(__cdecl *Flush_t)(int, int, int, int, int, int, int, void *, int, int, void *, int, int, int);
static Flush_t g_flushTramp;

// The door scope's lens (a fan around its centre) shows the screen copy: sample the middle of the wide picture.
static void FixLensUVs(void)
{
    int b;
    if (g_scale2d <= 1.001f) return;
    for (b = 0; b < 2; ++b) {
        DWORD n = *(DWORD *)(0x12F6DC8 + b * 12), i;
        float *v = (float *)(0x12F6DE0 + b * 280000), c;
        if (!n || n > 10000) continue;
        c = v[5];
        for (i = 0; i < n; ++i, v += 7) v[5] = c + (v[5] - c) / g_scale2d;
    }
}

// ---------------------------------------------------------------- v1.6: full-screen fills cover the whole picture
// User (2026-11-01): "Make the whole background dark while inspecting an item in all ratios ... the darkness gets
// padded in a pillar design on both sides when ratios are not 4:3". Sprites never pass the flush: every sprite draw goes
// 565840 / 565890 -> 564A00 / 564BC0 / 564F00 / 5652D0 and draws at once with the constants in force, the interface set,
// so the item examine screen's darkening (50EE30: a type-2 sprite over the whole 512x448 canvas, untextured, subtractive
// blend, drawn by `call 565890` at 50EF08) covered only the 4:3 middle. Same for the black full-canvas sprites 522820 /
// 5215D0 and the fade sprite 529180. Fills are drawn with the native set; pictures (textured sprites) and every other
// GUI element stay 4:3:
//   50EF08 `call 565890`        the examine darkening, always native (DimDraw)
//   565840 / 565890 (inline)    type-2 sprites, unrotated, untextured (or a screen copy), covering the whole canvas
//   flush 564210                untextured batches of at most 6 vertices (one quad) covering the whole canvas
// Type-2 sprite object (564F00): +0 / +4 blend, +18h texture (the game's texture header, 0 = none), +28h layer, +30h /
// +34h x / y, +38h / +3Ch w / h, +50h..+5Ch colours, +60h angle, +64h / +68h scale, +6Ch / +70h pivot (int). Its
// corners: x0 = x - pivot.x * scale.x, x1 = x0 + w * scale.x (y alike), in the 512x448 canvas, or in 640x480 while
// [137F980] is set (564F00 then draws with shader 10000001, c94 / c95). Flush shader spaces: 0 = c2 / c3 (512x448 in GS
// units, offset 1792 / 1824), 1 = c94 / c95 (640x480), 2 = c2 / c3 perspective (world, native already), 3 = c40 / c41.
// [Window] Menus43Log=1: every sprite / batch at least 90 % of the canvas wide is logged once per call site and decision.
//
// v1.8, [Window] MenuPillars. User (2026-11-02): "Make background of scrap book and memos full or black pillared on
// wider aspect ratios." The screens that hide the world behind a full-screen picture draw it as a type-2 sprite over
// the whole canvas, textured, with alpha blending OFF (sprite flag 40h clear; 403870 maps flag 40h to
// D3DRS_ALPHABLENDENABLE, 08h to the shade mode): on the PS2 the picture covered the whole screen, here it covers the
// 4:3 middle and the live 3D world shows beside it. Such a sprite gets the sides cleared to black just before it
// (BlackSides, as beside the peep masks); the picture stays undistorted and everything drawn after it (text, cursor)
// is in the 4:3 middle anyway. Found statically (GOG v7), all through 565840:
//   memo reader      50A200 draws object 50A300 at 50A2B2 / 50A2EE, picture from table 6538E0 (snap_1stlet.bin ...)
//   Scrap Book       520BB0: state block 1090A10, +38h tray (F006F1DF sub 1) at 520BF2, +B0h the memo's picture
//                    (table 5C56A0) at 520C49; objects built by 51FC70 / 51FCF0
//   item box         510680 (snap_itembox.bin) at 510767;  map 51AA30 (object +698h, pivot centre)
//   puzzle close-ups 4D5990 (snap_dial.bin), 4DC600 (4DC8C0, 4E9330);  game over 50D8B0 (snap_gameover.bin, 50D99A)
//   results          5270B0 (snap_result0n.bin, 527185);  opening pictures 518950 / 518D20 (snap_opening.bin)
// Blended full-canvas pictures are left alone (they are meant to show what is under them): the Options backdrop 523F40
// (over the full-width wobbling grid), the save screen 4CCC10 and the demo022 flash 507C00.
#define CANVAS_W 512.0f
#define CANVAS_H 448.0f
#define BATCH_SLOTS 2
#define BATCH_TYPE(b) (*(DWORD *)(0x12F6DC0 + (b) * 12))   // primitive kind: 0 strip, 1 fan, 2 / 3 list, 4 lines
#define BATCH_USED(b) (*(DWORD *)(0x12F6DC4 + (b) * 12))
#define BATCH_COUNT(b) (*(DWORD *)(0x12F6DC8 + (b) * 12))
#define BATCH_VERTS(b) ((const float *)(0x12F6DE0 + (b) * 280000))  // 7 floats: x y z w colour u v
static DWORD g_diagKeys[256];
static int g_diagN;

static int DiagNew(DWORD key)  // game thread only
{
    int i;
    for (i = 0; i < g_diagN; ++i)
        if (g_diagKeys[i] == key) return 0;
    if (g_diagN >= (int)(sizeof g_diagKeys / sizeof g_diagKeys[0])) return 0;
    g_diagKeys[g_diagN++] = key;
    return 1;
}

static int IsScreenTex(const void *t)
{
    int i;
    for (i = 0; t && i < (int)(sizeof kScreenTex / sizeof kScreenTex[0]); ++i)
        if (*kScreenTex[i] == t) return 1;
    return 0;
}

// The D3D texture a game texture header holds (42AAC0 reads it the same way; nothing is created here), or NULL
static const void *HeaderTex(const BYTE *hdr)
{
    DWORD off = hdr ? *(const DWORD *)(hdr + 0x0C) : 0;
    return off ? *(void *const *)(hdr + off + 0x1C) : NULL;
}

#define SPR_FILL 1    // draw with the native constants (full-width fill)
#define SPR_PILLARS 2 // opaque full-screen picture: black sides first (v1.8)
static void BlackSides(void);

// ---------------------------------------------------------------- v2.1 SAVE PILLARS (begin)
// User (2026-11-04): "the in-game SAVE menu ... the live 3D world is visible in the side areas left and right. It needs
// black pillar boxes at the sides". The save screen's picture is snap_save.bin (resource F000F00A, exe file table
// 0x69F698 entry 10), drawn over the whole 512x448 canvas but WITH alpha blending on (sprite flags 156h: 40h set, blend
// 44h / 80h, colour 80808080h), so v1.8's opaque-only rule left it alone and the world shows beside the 4:3 picture.
// These call sites are full-screen pictures that hide the world on the PS2 although they are drawn blended; with
// [Window] MenuPillars=1 they get the same black sides (BlackSides) as the opaque pictures:
//   sprite 565840, return 4CCD2A   in-game save screen: 4CCCE0 draws sprite object 0xFD14F8 (built by 4CCC10) while
//                                  [0xFD1574] != 0. One flow for every in-game save: the save points (event handler
//                                  4CCFE0 in table 0x6181CC registers 4CCDC0), Room 302's notebook, and the clear-data
//                                  save after the ending (4EFA30 state 3 -> 4CD060 -> 4CCDC0). The menu itself is
//                                  571AC0 / 571B10 -> task 571130 (slot list in the 4:3 middle).
//   flush 564210, return 570584    title Load screen (top mode 4, 42CF10 -> 571B40): 570490 draws snap_save.bin as one
//                                  textured quad through the flush (called from 5725F9 when the screen state > 2).
// Not listed (left alone on purpose): the Options backdrop 523F40 (it lies over the full-width wobbling grid 552FA0,
// so the sides show the grid, not the world) and the demo022 flash 507C00 (meant to show what is under it).
static const DWORD kPillarSpriteSites[] = { 0x4CCD2A };
static const DWORD kPillarFlushSites[] = { 0x570584 };

static int SiteIn(const DWORD *list, int n, DWORD site)
{
    int i;
    for (i = 0; i < n; ++i)
        if (list[i] == site) return 1;
    return 0;
}
#define PILLAR_SPRITE_SITE(s) SiteIn(kPillarSpriteSites, (int)(sizeof kPillarSpriteSites / sizeof kPillarSpriteSites[0]), (s))
#define PILLAR_FLUSH_SITE(s) SiteIn(kPillarFlushSites, (int)(sizeof kPillarFlushSites / sizeof kPillarFlushSites[0]), (s))
// ---------------------------------------------------------------- v2.1 SAVE PILLARS (end)

// 565840 / 565890 (cdecl obj, type): SPR_FILL when this sprite is a full-screen fill to draw with the native constants,
// SPR_PILLARS when it is an opaque full-screen picture that keeps 4:3 with black sides
static int SpriteFill(const BYTE *o, int type, DWORD site)
{
    float sx, sy, ax, bx, ay, by, x0, x1, y0, y1, angle, cw, ch;
    const BYTE *tex;
    int full, plain, fill, opaque, pillars;
    const char *why;
    if (!o || type != 2 || (!g_menus43Log && (!g_haveConsts || g_scale2d <= 1.001f))) return 0;
    sx = *(const float *)(o + 0x64);
    sy = *(const float *)(o + 0x68);
    ax = *(const float *)(o + 0x30) - (float)*(const int *)(o + 0x6C) * sx;
    ay = *(const float *)(o + 0x34) - (float)*(const int *)(o + 0x70) * sy;
    bx = ax + *(const float *)(o + 0x38) * sx;
    by = ay + *(const float *)(o + 0x3C) * sy;
    x0 = ax < bx ? ax : bx; x1 = ax < bx ? bx : ax;
    y0 = ay < by ? ay : by; y1 = ay < by ? by : ay;
    angle = *(const float *)(o + 0x60);
    cw = *(const int *)0x137F980 ? 640.0f : CANVAS_W;
    ch = *(const int *)0x137F980 ? 480.0f : CANVAS_H;
    tex = *(const BYTE *const *)(o + 0x18);
    full = angle == 0.0f && x0 <= 0.5f && y0 <= 0.5f && x1 >= cw - 0.5f && y1 >= ch - 0.5f;
    plain = !tex || (full && IsScreenTex(HeaderTex(tex)));
    fill = full && plain && g_haveConsts && g_scale2d > 1.001f && g_ifaceSet;
    opaque = !(*(const DWORD *)(o + 8) & 0x40);  // flag 40h = alpha blending (403870)
    opaque |= PILLAR_SPRITE_SITE(site);          // v2.1 SAVE PILLARS: blended, but hides the world (save screen)
    pillars = full && !plain && opaque && g_menuPillars && g_haveConsts && g_scale2d > 1.001f;
    if (g_menus43Log && x1 - x0 >= 0.9f * cw) {
        why = fill ? (tex ? "full width (screen copy)" : "full width (untextured fill)")
              : !g_haveConsts || g_scale2d <= 1.001f ? "4:3 shape, nothing to change"
              : !full ? (angle != 0.0f ? "4:3 (rotated)" : "4:3 (not the whole canvas)")
              : pillars ? (PILLAR_SPRITE_SITE(site) ? "4:3 picture with black sides (save screen)"  // v2.1
                                                    : "4:3 picture with black sides (opaque)")
              : !plain ? (opaque ? "4:3 (textured picture; MenuPillars off)" : "4:3 (textured picture, blended)")
              : "full width already";
        if (DiagNew(site | (fill ? 0x80000000u : 0) | (tex ? 0x40000000u : 0) | (pillars ? 0x20000000u : 0) | 0x10000000u))
            Log("menus 4:3 log: sprite from %08lX: x %.1f..%.1f y %.1f..%.1f of %.0fx%.0f, angle %g, texture %p, colour %08lX, "
                "blend %08lX/%08lX, layer %u -> %s", site, x0, x1, y0, y1, cw, ch, angle, tex, *(const DWORD *)(o + 0x50),
                *(const DWORD *)o, *(const DWORD *)(o + 4), *(const WORD *)(o + 0x28), why);
    }
    return (fill ? SPR_FILL : 0) | (pillars ? SPR_PILLARS : 0);
}

typedef int(__cdecl *SprDraw_t)(void *, int);
static SprDraw_t g_spr40Tramp, g_spr90Tramp;
static int g_inDimDraw;  // DimDraw is calling 565890 (the log then names the game's site 50EF08, not this dll)

static int __cdecl SpriteDraw40(void *o, int type)  // 565840 (inline hook; the return address is the game's call site)
{
    const int k = SpriteFill((const BYTE *)o, type, (DWORD)(ULONG_PTR)_ReturnAddress()), fill = k & SPR_FILL;
    int r;
    if (k & SPR_PILLARS) BlackSides();  // v1.8
    if (fill) Apply2D(&g_native);
    r = g_spr40Tramp(o, type);
    if (fill) Apply2D(&g_iface);
    return r;
}

static int __cdecl SpriteDraw90(void *o, int type)  // 565890 (ordering-table variant; its only caller is 50EF08)
{
    const int k = SpriteFill((const BYTE *)o, type, g_inDimDraw ? 0x50EF0D : (DWORD)(ULONG_PTR)_ReturnAddress());
    const int fill = k & SPR_FILL;
    int r;
    if (k & SPR_PILLARS) BlackSides();  // v1.8
    if (fill) Apply2D(&g_native);
    r = g_spr90Tramp(o, type);
    if (fill) Apply2D(&g_iface);
    return r;
}

// 50EF08 (was `call 565890`, cdecl obj, type): the item examine screen's darkening, always over the whole picture
// v3.22.0 G1 (user item 1, staging/feedback_v322/re_gates.md 1): only the item ring's description box draws this dim
// (0x50EE30 from 50F380(0, 0)); the user wants the dark inside the white box only, the filtered scene around it. While
// the box is visible ([0x108DCF0]) the sprite takes the box rect (object 0x108DCF8: floats x, y, w, h), inset 2, as a 4:3
// interface element, and its geometry is put back after the call (565890 builds the vertices at once). A rect outside
// the canvas -> the stock full-screen dim (logged).
static int __cdecl DimDraw(void *o, int type)
{
    int nat = g_haveConsts && g_scale2d > 1.001f && g_ifaceSet, r, boxed = 0;
    BYTE *s = (BYTE *)o, keep[0x74];
    if (s && *(volatile int *)0x108DCF0 && *(volatile int *)0x108DDC4) {
        static int s_logged;
        const float *b = (const float *)0x108DCF8;
        const int menu = *(volatile int *)0x137F980 != 0;
        const float cw = menu ? 640.0f : CANVAS_W, ch = menu ? 480.0f : CANVAS_H;
        __try {
            const float x = b[0], y = b[1], w = b[2], h = b[3];
            const int ok = _finite(x + y + w + h) && w >= 16.0f && h >= 16.0f && x >= -1.0f && y >= -1.0f && x + w <= cw + 1.0f &&
                           y + h <= ch + 1.0f;
            if (!s_logged++)
                Log("item ring: description box %.1f, %.1f %.1f x %.1f; dim sprite %.1f, %.1f %.1f x %.1f (pivot %d, %d; %s canvas): %s",
                    x, y, w, h, *(float *)(s + 0x30), *(float *)(s + 0x34), *(float *)(s + 0x38), *(float *)(s + 0x3C),
                    *(int *)(s + 0x6C), *(int *)(s + 0x70), menu ? "640x480" : "512x448",
                    ok ? "the dark stays inside the box" : "implausible: the stock full-screen dim");
            if (ok) {
                memcpy(keep, s, sizeof keep);
                *(float *)(s + 0x30) = x + 2.0f;
                *(float *)(s + 0x34) = y + 2.0f;
                *(float *)(s + 0x38) = w - 4.0f;
                *(float *)(s + 0x3C) = h - 4.0f;
                *(float *)(s + 0x64) = *(float *)(s + 0x68) = 1.0f;
                *(int *)(s + 0x6C) = *(int *)(s + 0x70) = 0;
                boxed = 1;
                nat = 0;  // the box is a 4:3 interface element
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            boxed = 0;
        }
    } else if (s && *(volatile int *)0x108DDC4 && !*(volatile int *)0x108DCF0) {
        return 0;  // the description's dim while its box is not (yet / any more) shown: none
    }
    if (nat) Apply2D(&g_native);
    g_inDimDraw = !boxed;  // (boxed: the 565890 hook treats it as an ordinary interface sprite)
    r = ((SprDraw_t)0x565890)(o, type);  // the 565890 hook then sees the native set and leaves it
    g_inDimDraw = 0;
    if (boxed) memcpy(s, keep, sizeof keep);
    if (nat) Apply2D(&g_iface);
    return r;
}

// The batch about to be drawn (the used slots): its extent, vertex count and first colour. 0 when it has none or uses
// lines / a w other than 1 (space 0).
static int BatchExtent(BYTE space, DWORD maxCount, float *x0, float *y0, float *x1, float *y1, DWORD *count, DWORD *colour,
                       int *tris)
{
    int b, any = 0;
    DWORD total = 0;
    *count = 0;
    *tris = 1;
    *colour = 0;
    *x0 = *y0 = 1e30f;
    *x1 = *y1 = -1e30f;
    for (b = 0; b < BATCH_SLOTS; ++b)  // the count first: the fill test reads at most 6 vertices
        if (BATCH_USED(b)) total += BATCH_COUNT(b) > 10000 ? 0x10000 : BATCH_COUNT(b);
    if (!total || total > maxCount) return 0;
    for (b = 0; b < BATCH_SLOTS; ++b) {
        DWORD n = BATCH_COUNT(b), i;
        const float *v = BATCH_VERTS(b);
        if (!BATCH_USED(b) || !n) continue;
        if (BATCH_TYPE(b) > 3) *tris = 0;
        if (!any) *colour = *(const DWORD *)(v + 4);
        any = 1;
        *count += n;
        for (i = 0; i < n; ++i, v += 7) {
            if (space == 0 && fabsf(v[3] - 1.0f) > 0.01f) *tris = 0;
            if (v[0] < *x0) *x0 = v[0];
            if (v[0] > *x1) *x1 = v[0];
            if (v[1] < *y0) *y0 = v[1];
            if (v[1] > *y1) *y1 = v[1];
        }
    }
    return any;
}

static void CanvasOfSpace(BYTE space, float *w, float *h)
{
    *w = space == 1 ? 640.0f : CANVAS_W;
    *h = space == 1 ? 480.0f : CANVAS_H;
}

// Untextured, one quad (<= 6 vertices, triangles), covering the whole canvas of its space (by its extent, so the GS
// offset of space 0 does not matter)
static int FullCanvasBatch(BYTE space)
{
    float x0, y0, x1, y1, w, h;
    DWORD n, col;
    int tris;
    if (space > 1 || !BatchExtent(space, 6, &x0, &y0, &x1, &y1, &n, &col, &tris)) return 0;
    CanvasOfSpace(space, &w, &h);
    return tris && n >= 3 && n <= 6 && x1 - x0 >= w - 0.5f && y1 - y0 >= h - 0.5f;
}

static void FlushDiag(DWORD site, BYTE space, const void *tex, const void *tex2, int world, int fill)
{
    float x0, y0, x1, y1, w, h;
    DWORD n, col;
    int tris;
    const char *why;
    if (space > 1 || !BatchExtent(space, 20000, &x0, &y0, &x1, &y1, &n, &col, &tris)) return;
    CanvasOfSpace(space, &w, &h);
    if (x1 - x0 < 0.9f * w) return;
    why = fill ? "full width (untextured fill)" : world ? "full width (world / screen copy / target)"
          : !g_haveConsts || g_scale2d <= 1.001f ? "4:3 shape, nothing to change"
          : tex || tex2 ? "4:3 (textured)" : n > 6 ? "4:3 (more than one quad)" : "4:3 (not the whole canvas)";
    if (DiagNew(site | (world ? 0x80000000u : 0) | ((tex || tex2) ? 0x40000000u : 0) | 0x20000000u))
        Log("menus 4:3 log: batch from %08lX: space %u, %lu vertices, x %.1f..%.1f y %.1f..%.1f (canvas %.0fx%.0f), "
            "textures %p / %p, colour %08lX -> %s", site, space, n, x0, x1, y0, y1, w, h, tex, tex2, col, why);
}

static void __cdecl Flush(int a0, int a1, int a2, int a3, int a4, int a5, int a6, void *tex, int a8, int a9, void *tex2,
                          int a11, int a12, int mode)
{
    DWORD site = (DWORD)(ULONG_PTR)_ReturnAddress();
    BYTE space = (BYTE)mode;
    int world = 0, fill = 0, i;
    if (g_haveConsts && g_scale2d > 1.001f && space != 3) {
        world = space == 2;
        for (i = 0; !world && i < (int)(sizeof kWorldSites / sizeof kWorldSites[0]); ++i) world = site == kWorldSites[i];
        if (!world) world = SiteIn(g_worldSites324, WORLD324_N, site);  // v3.2.4 WORLD EFFECTS
        if (!world && space == 1)
            for (i = 0; !world && i < (int)(sizeof kScreenTex / sizeof kScreenTex[0]); ++i)
                world = *kScreenTex[i] && (*kScreenTex[i] == tex || *kScreenTex[i] == tex2);
        if (!world && !tex && !tex2 && space <= 1) world = fill = FullCanvasBatch(space);  // v1.6
        if (!world) world = DrawingOffscreen();
    }
    if (g_menus43Log) FlushDiag(site, space, tex, tex2, world, fill);
    if (world) Apply2D(&g_native);
    else if (g_haveConsts && !g_ifaceSet) Apply2D(&g_iface);
    if (site == LENS_SITE) FixLensUVs();
    if (g_menuPillars && g_haveConsts && g_scale2d > 1.001f && PILLAR_FLUSH_SITE(site)) BlackSides();  // v2.1 SAVE PILLARS
    g_flushTramp(a0, a1, a2, a3, a4, a5, a6, tex, a8, a9, tex2, a11, a12, mode);
    if (world) Apply2D(&g_iface);
}

// Cinema bars (4EFC56 `call 4F4870`): full width, as on the PS2
static void __cdecl CinemaBars(void)
{
    if (g_haveConsts) Apply2D(&g_native);
    ((void(__cdecl *)(void))0x4F4870)();
    if (g_haveConsts) Apply2D(&g_iface);
}

// Masks that hid the whole PS2 screen (Room 302 wall hole, door scope, Water Prison peep holes) are 4:3 sprites: the
// sides beside them are cleared to black, so the wider world does not show there.
// v3.41 VW341: the widths from views_sim.h (exact integers: left floor(L) + 1, right floor(L)); v3.40's ceil of the
// float L on both sides left a 1 px column of the world beside the picture at some sizes (1366x768, 1360x768: the
// float edge lands a hair right of a whole L) and at whole L once the GPU's edge moves by 1/256 px (views_sim_test).
// v3.42 EG342: the user's v3.41 shots (memo, window view) still showed the world in column 0 and row 0 of the sides:
// the Clear of [0, left) lost them on the user's backend (dgVoodoo2, Direct3D 11) like the canvas edges did. After the
// Clear the sides are drawn again as two black quads (PillarQuads) that run 1 px past the screen edges; widths
// ceil(L + 0.5) on both sides (views_sim.h, beside the aligned 4:3 picture). PixelAlign2D=0: the Clear only.
typedef HRESULT(STDMETHODCALLTYPE *EgSetRS_t)(void *, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *EgGetRS_t)(void *, DWORD, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *EgGetTex_t)(void *, DWORD, void **);
typedef HRESULT(STDMETHODCALLTYPE *EgSetTex_t)(void *, DWORD, void *);
typedef HRESULT(STDMETHODCALLTYPE *EgSetTSS_t)(void *, DWORD, DWORD, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *EgGetTSS_t)(void *, DWORD, DWORD, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *EgDrawUP_t)(void *, DWORD, UINT, const void *, UINT);
typedef HRESULT(STDMETHODCALLTYPE *EgSetDW_t)(void *, DWORD);
typedef HRESULT(STDMETHODCALLTYPE *EgGetDW_t)(void *, DWORD *);
typedef HRESULT(STDMETHODCALLTYPE *EgGetStream_t)(void *, UINT, void **, UINT *);
typedef HRESULT(STDMETHODCALLTYPE *EgSetStream_t)(void *, UINT, void *, UINT);
// D3D8 vtable (as items_draw.inc): SetRenderState 50, GetRenderState 51, GetTexture 60, SetTexture 61, GetTSS 62,
// SetTSS 63, DrawPrimitiveUP 72, SetVertexShader 76, GetVertexShader 77, SetStreamSource 83, GetStreamSource 84,
// SetPixelShader 88, GetPixelShader 89
enum { EGV_SETRS = 50, EGV_GETRS = 51, EGV_GETTEX = 60, EGV_SETTEX = 61, EGV_GETTSS = 62, EGV_SETTSS = 63, EGV_DRAWUP = 72,
       EGV_SETVS = 76, EGV_GETVS = 77, EGV_SETSTREAM = 83, EGV_GETSTREAM = 84, EGV_SETPS = 88, EGV_GETPS = 89 };
typedef struct { float x, y, z, rhw; DWORD c; } EgVtx;  // FVF 0x44: XYZRHW | DIFFUSE
static volatile LONG g_egQuadFails;

// Two opaque black quads (Direct3D 9 screen coordinates from VwPillarQuads); every state touched is read first and put
// back, the shaders, texture 0 and stream 0 too. 1 = drawn.
static int PillarQuads(void *dev, int w, int h, int left, int right)
{
    static const DWORD kRs[] = { 7 /*ZENABLE*/, 14 /*ZWRITEENABLE*/, 15 /*ALPHATESTENABLE*/, 27 /*ALPHABLENDENABLE*/,
                                 28 /*FOGENABLE*/, 22 /*CULLMODE*/, 52 /*STENCILENABLE*/, 137 /*LIGHTING*/,
                                 168 /*COLORWRITEENABLE*/, 8 /*FILLMODE*/ };
    static const DWORD kRsSet[] = { 0, 0, 0, 0, 0, 1 /*NONE*/, 0, 0, 0xF, 3 /*SOLID*/ };
    static const DWORD kTss[][2] = { { 0, 1 }, { 0, 2 }, { 0, 4 }, { 0, 5 }, { 1, 1 }, { 1, 4 } };
    static const DWORD kTssSet[] = { 2 /*SELECTARG1*/, 0 /*DIFFUSE*/, 2, 0, 1 /*DISABLE*/, 1 };
    DWORD rs[sizeof kRs / sizeof kRs[0]], tss[sizeof kTssSet / sizeof kTssSet[0]], vs = 0, ps = 0;
    void *oldTex = NULL, *oldVb = NULL;
    UINT oldStride = 0;
    float q[2][4];
    EgVtx v[12];
    HRESULT hr = S_OK;
    int i, k, n = VwPillarQuads(w, h, left, right, q);
    if (n <= 0) return 0;
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i)
        if (FAILED(((EgGetRS_t)VT(dev, EGV_GETRS))(dev, kRs[i], &rs[i]))) return 0;  // a pure device: the Clear only
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        if (FAILED(((EgGetTSS_t)VT(dev, EGV_GETTSS))(dev, kTss[i][0], kTss[i][1], &tss[i]))) return 0;
    if (FAILED(((EgGetDW_t)VT(dev, EGV_GETVS))(dev, &vs)) || FAILED(((EgGetDW_t)VT(dev, EGV_GETPS))(dev, &ps))) return 0;
    if (FAILED(((EgGetTex_t)VT(dev, EGV_GETTEX))(dev, 0, &oldTex))) oldTex = NULL;  // AddRef'd
    if (FAILED(((EgGetStream_t)VT(dev, EGV_GETSTREAM))(dev, 0, &oldVb, &oldStride))) oldVb = NULL;  // AddRef'd
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((EgSetRS_t)VT(dev, EGV_SETRS))(dev, kRs[i], kRsSet[i]);
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        ((EgSetTSS_t)VT(dev, EGV_SETTSS))(dev, kTss[i][0], kTss[i][1], kTssSet[i]);
    ((EgSetDW_t)VT(dev, EGV_SETPS))(dev, 0);
    ((EgSetDW_t)VT(dev, EGV_SETVS))(dev, 0x44);
    ((EgSetTex_t)VT(dev, EGV_SETTEX))(dev, 0, NULL);
    for (k = 0; k < n; ++k) {  // two triangles a quad: (x0,y0) (x1,y0) (x0,y1) / (x1,y0) (x1,y1) (x0,y1)
        static const int kCx[6] = { 0, 2, 0, 2, 2, 0 }, kCy[6] = { 1, 1, 3, 1, 3, 3 };
        for (i = 0; i < 6; ++i) {
            EgVtx *p = &v[k * 6 + i];
            p->x = q[k][kCx[i]];
            p->y = q[k][kCy[i]];
            p->z = 0.0f;
            p->rhw = 1.0f;
            p->c = 0xFF000000u;
        }
    }
    hr = ((EgDrawUP_t)VT(dev, EGV_DRAWUP))(dev, 4 /*TRIANGLELIST*/, (UINT)(n * 2), v, sizeof(EgVtx));
    ((EgSetTex_t)VT(dev, EGV_SETTEX))(dev, 0, oldTex);
    if (oldVb) {
        ((EgSetStream_t)VT(dev, EGV_SETSTREAM))(dev, 0, oldVb, oldStride);
        ((Release_t)VT(oldVb, VT_RELEASE))(oldVb);
    }
    if (oldTex) ((Release_t)VT(oldTex, VT_RELEASE))(oldTex);
    ((EgSetDW_t)VT(dev, EGV_SETVS))(dev, vs);
    ((EgSetDW_t)VT(dev, EGV_SETPS))(dev, ps);
    for (i = 0; i < (int)(sizeof kTssSet / sizeof kTssSet[0]); ++i)
        ((EgSetTSS_t)VT(dev, EGV_SETTSS))(dev, kTss[i][0], kTss[i][1], tss[i]);
    for (i = 0; i < (int)(sizeof kRs / sizeof kRs[0]); ++i) ((EgSetRS_t)VT(dev, EGV_SETRS))(dev, kRs[i], rs[i]);
    if (FAILED(hr)) {
        if (LogQuota(&g_egQuadFails, 3))
            LogAnomaly("2D edges", "the black-side quads failed (hr %08lX, %dx%d, %d | %d px): the Clear alone covers the sides",
                       (unsigned long)hr, w, h, left, right);
        return 0;
    }
    return 1;
}

static void BlackSides(void)
{
    void *dev = DEV;
    int w = *(int *)0x5D524C, h = *(int *)0x5D5250, left, right;  // v3.41 VW341
    Rect8 r[2];
    Viewport8 vp, full;
    if (!dev || g_scale2d <= 1.001f || DrawingOffscreen()) return;
    if (!VwPillars(w, h, &left, &right) || left <= 0) return;  // v3.41 VW341
    r[0].x1 = 0; r[0].y1 = 0; r[0].x2 = left; r[0].y2 = h;
    r[1].x1 = w - right; r[1].y1 = 0; r[1].x2 = w; r[1].y2 = h;  // v3.41 VW341
    full.x = full.y = 0; full.w = (DWORD)w; full.h = (DWORD)h; full.minZ = 0.0f; full.maxZ = 1.0f;
    if (FAILED(((Viewport_t)VT(dev, VT_GETVP))(dev, &vp))) vp = full;
    ((Viewport_t)VT(dev, VT_SETVP))(dev, &full);
    ((Clear_t)VT(dev, VT_CLEAR))(dev, right > 0 ? 2 : 1, r, 1 /* D3DCLEAR_TARGET */, 0xFF000000, 1.0f, 0);  // v3.41 VW341
    if (g_pixAlign) PillarQuads(dev, w, h, left, right);  // v3.42 EG342: column 0 / row 0 and the inner edges, for sure
    ((Viewport_t)VT(dev, VT_SETVP))(dev, &vp);
}

static int(__cdecl *g_peepTramp)(int), (__cdecl *g_prisonTramp)(BYTE *);
static int __cdecl Peep(int hole)
{
    int r = g_peepTramp(hole);
    BlackSides();
    return r;
}
static int __cdecl PrisonPeep(BYTE *obj)
{
    int r = g_prisonTramp(obj);
    if (*(void **)(obj + 0x408)) BlackSides();
    return r;
}

// ---------------------------------------------------------------- v3.41 VW341: Room 302's window views at 4:3
// User (2026-11-22): "Non 4:3 aspects allow you to see outside the window frame." Looking out of the living-room or
// bedroom window is one event, records 0x69843C and 0x698468 (both {0x54C030, init 0x54C050, update 0x54C190, draw
// 0x54CC10, end 0x54C5F0}; task type bit 0 = which window, decomp src/haunting/evlook_54b9e0.c). Its draw 0x54CC10
// (cdecl task, from state 3 on) draws the outside scenery (em 0x422BA0, flyers 0x550C70, 0x54E4C0, 0x54DE60) full
// width and then the window frame EventDraw2DFrame 0x54C690, a 2D picture of the 512-wide canvas that Menus43 keeps 4:3:
// in a wide shape the street beyond the frame shows beside it (the screenshots 2 / 3). The room is not drawn during the
// view (gGame.unk_1D4), so the sides are cleared to black (BlackSides) right after the event's draw, whatever the
// aspect or FOV; the cinema bars, subtitles and text are drawn after it (0x4EFC0x EventTickAll, then 0x4EFC56 ...).
// Hook: 0x54CC10's first 6 bytes `56 57 8B 7C 24 0C` (push esi / push edi / mov edi,[esp+0Ch]) into a trampoline;
// not a Playable Eileen hook, no cheat-table span (ct_conflicts.py --span 0x54CC10-0x54CC16). Hidden key
// [Window] WindowView43=1 (default; 0 = the v3.40 view, full width). Peepholes, the door scope and the Water Prison
// peeps keep their own masks and BlackSides (Peep / PrisonPeep above).
static int(__cdecl *g_lookTramp)(BYTE *);
static DWORD g_lookLast;
static volatile LONG g_lookLogs;
static int __cdecl LookOutDraw(BYTE *task)
{
    const int r = g_lookTramp(task);
    if (task && *(const WORD *)(task + 0x410) >= 3 && g_haveConsts && g_scale2d > 1.001f) {
        const DWORD now = GetTickCount();
        if (now - g_lookLast > 3000 && LogQuota(&g_lookLogs, 30)) {
            int w = *(int *)0x5D524C, h = *(int *)0x5D5250, left = 0, right = 0;
            VwPillars(w, h, &left, &right);
            LogV("window views: Room 302's window view: black sides %d | %d px beside the 4:3 view (%dx%d)", left, right, w, h);
        }
        g_lookLast = now;  // v3.41 REV341 VW341-001: the last draw, so the line above comes once per view (a view = draws
                           // with no gap over 3 s), not every 3 s inside one view (that would only repeat it)
        BlackSides();
    }
    return r;
}

// Mouse: menus are squeezed into the 4:3 area, so a menu x maps to 256 + (x - 256) * S in the game's 512 space
static float ToIface(float x) { return 256.0f + (x - 256.0f) * g_scale2d; }
static float FromIface(float x) { return 256.0f + (x - 256.0f) / g_scale2d; }
static void(__cdecl *g_cursorTramp)(float *, float *), (__cdecl *g_setCursorTramp)(double, double);
static int(__cdecl *g_hitFTramp)(float *), (__cdecl *g_hitITramp)(int *), (__cdecl *g_prevTramp)(void), (__cdecl *g_nextTramp)(void);

static void __cdecl MenuCursor(float *x, float *y)
{
    g_cursorTramp(x, y);
    if (x) *x = ToIface(*x);
}
static void __cdecl SetMenuCursor(double x, double y) { g_setCursorTramp((double)FromIface((float)x), y); }
static int __cdecl HitRectF(float *rc)
{
    float r[4];
    r[0] = FromIface(rc[0]); r[1] = rc[1]; r[2] = rc[2] / g_scale2d; r[3] = rc[3];
    return g_hitFTramp(r);
}
static int __cdecl HitRectI(int *rc)
{
    int r[4];
    r[0] = (int)floorf(FromIface((float)rc[0]) + 0.5f); r[1] = rc[1]; r[2] = (int)floorf(rc[2] / g_scale2d + 0.5f); r[3] = rc[3];
    return g_hitITramp(r);
}
static int CursorIn(float x0, float y0, float w, float h)
{
    float x = 0, y = 0;
    MenuCursor(&x, &y);
    return x >= x0 && x < x0 + w && y >= y0 && y < y0 + h;
}
// the Options page arrows at the top (512x448 space)
static int __cdecl PrevPage(void) { return g_scale2d > 1.001f ? CursorIn(33.6f, 39.2f, 48.0f, 28.0f) : g_prevTramp(); }
static int __cdecl NextPage(void) { return g_scale2d > 1.001f ? CursorIn(429.6f, 39.2f, 48.0f, 28.0f) : g_nextTramp(); }

// Copies n stolen bytes (no relative operands) into a trampoline that jumps back to va + n; then jmp va -> hook.
static void *InlineHook(DWORD va, int n, void *hook)
{
    BYTE *t;
    DWORD rel;
    if (!g_tramp) g_tramp = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_tramp || g_trampUsed + n + 5 > 4096) return NULL;
    t = g_tramp + g_trampUsed;
    memcpy(t, (void *)(ULONG_PTR)va, n);
    t[n] = 0xE9;
    rel = va + n - ((DWORD)(ULONG_PTR)(t + n) + 5);
    memcpy(t + n + 1, &rel, 4);
    if (!PokeBranch(va, 0xE9, hook, n)) return NULL;  // v3.41 REV341 VW341-R1: not hooked (the slot is not taken)
    g_trampUsed += (n + 5 + 15) & ~15;
    return t;
}

// v1.6: the full-screen fills (section above Flush). Each site is checked on its own: one that another mod changed
// leaves only that part out (the flush rule needs no site of its own).
static void ApplyFills(void)
{
    static const Site kFillSites[] = {
        { 0x50EF08, { 0xE8, 0x83, 0x69, 0x05, 0x00 }, 5 },                    // call 565890 (examine darkening)
        { 0x565840, { 0x8B, 0x44, 0x24, 0x08, 0x83, 0xF8, 0x03 }, 7 },        // sprite draw: mov eax,[esp+8] / cmp eax,3
        { 0x565890, { 0x51, 0x53, 0x56, 0x57, 0x33, 0xFF }, 6 },              // sprite draw (OT): push ecx..edi / xor edi,edi
    };
    int i, ok[3], n = 0;
    for (i = 0; i < 3; ++i) {
        ok[i] = SiteOk(&kFillSites[i]);
        if (!ok[i]) {
            const BYTE *p = (const BYTE *)(ULONG_PTR)kFillSites[i].va;
            Log("menus 4:3: unexpected bytes at %08lX (%02X %02X %02X %02X ...): that full-screen fill stays 4:3",
                kFillSites[i].va, p[0], p[1], p[2], p[3]);
        }
    }
    if (ok[1] && (g_spr40Tramp = (SprDraw_t)InlineHook(0x565840, 7, SpriteDraw40)) != NULL) ++n;
    if (ok[2] && (g_spr90Tramp = (SprDraw_t)InlineHook(0x565890, 6, SpriteDraw90)) != NULL) ++n;
    if (ok[0] && PokeBranch(0x50EF08, 0xE8, DimDraw, 5)) ++n;
    Log("menus 4:3: full-screen fills (item examine darkening, black / fade sprites and quads) cover the whole picture "
        "(%d of 3 sites)%s", n, g_menus43Log ? "; Menus43Log on: full-width 2D draws are logged once per site" : "");
    if (g_menuPillars)
        Log("menus 4:3: full-screen pictures (memos, Scrap Book, item box, map, close-ups, save / load screens) get black sides%s",
            g_spr40Tramp ? "" : " -- not available (sprite draw 565840 not hooked)");
}

// v3.41 VW341: the window views' hook (LookOutDraw above), all or nothing on its one site
static void ApplyWindowView43(void)
{
    static const Site kLook = { 0x54CC10, { 0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C }, 6 };
    if (!g_view43) {
        Log("window views: [Window] WindowView43=0: Room 302's window views stay full width (the street beyond the frame shows in wide shapes)");
        return;
    }
    if (!SiteOk(&kLook)) {
        const BYTE *p = (const BYTE *)(ULONG_PTR)kLook.va;
        Log("window views: unexpected bytes at 0054CC10 (%02X %02X %02X %02X ...): Room 302's window views stay full width",
            p[0], p[1], p[2], p[3]);
        return;
    }
    g_lookTramp = (int(__cdecl *)(BYTE *))InlineHook(0x54CC10, 6, LookOutDraw);
    Log("window views: %s", g_lookTramp ? "Room 302's window views keep the 4:3 view with black sides in wide shapes (0054CC10 hooked)"
                                        : "not hooked (no trampoline space, or 0054CC10 could not be written): Room 302's "
                                          "window views stay full width");
}

static void ApplyMenus43(void)
{
    static const Site kSites[] = {
        { 0x4052B9, { 0xE8, 0xC2, 0xFA, 0xFF, 0xFF }, 5 },                                  // call 404D80
        { 0x564210, { 0xA1, 0xD4, 0x6D, 0x2F, 0x01 }, 5 },                                  // flush: mov eax,[12F6DD4]
        { 0x4EFC56, { 0xE8, 0x15, 0x4C, 0x00, 0x00 }, 5 },                                  // call 4F4870 (cinema bars)
        { 0x4F9BB0, { 0xE8, 0x8B, 0x79, 0xFF, 0xFF, 0x85, 0xC0, 0x74, 0x24 }, 9 },          // peep: call 4F1540
        { 0x4E8950, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x74 }, 9 },          // Water Prison peep
        { 0x4F96B4, { 0xE8, 0x57, 0xAB, 0x06, 0x00 }, 5 },                                  // lens flush call
        { 0x405980, { 0x83, 0xEC, 0x18, 0xDB, 0x05, 0x7C, 0xF2, 0x5F, 0x00 }, 9 },          // menu cursor get
        { 0x4058F0, { 0x83, 0xEC, 0x18, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00, 0x33, 0xC0 }, 11 },  // menu cursor set
        { 0x405760, { 0x83, 0xEC, 0x10, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00, 0x53, 0x55 }, 11 },  // hit rect (float)
        { 0x405690, { 0x83, 0xEC, 0x20, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00, 0x53, 0x55 }, 11 },  // hit rect (int)
        { 0x405570, { 0x83, 0xEC, 0x14, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00, 0x53, 0x55 }, 11 },  // previous page arrow
        { 0x405600, { 0x83, 0xEC, 0x14, 0x8B, 0x0D, 0x90, 0x7D, 0x6E, 0x00, 0x53, 0x55 }, 11 },  // next page arrow
    };
    int i, bad = 0;
    BYTE *t;
    DWORD rel;
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            const BYTE *p = (const BYTE *)(ULONG_PTR)kSites[i].va;
            Log("menus 4:3: unexpected bytes at %08lX (%02X %02X %02X %02X ...)", kSites[i].va, p[0], p[1], p[2], p[3]);
            ++bad;
        }
    if (bad) {
        Log("menus 4:3: %d site(s) differ from the GOG v7 exe: menus stay stretched in wide shapes", bad);
        return;
    }
    PokeBranch(0x4052B9, 0xE8, SetConstants2D, 5);
    CheckWorldSites324();  // v3.2.4 WORLD EFFECTS
    g_flushTramp = (Flush_t)InlineHook(0x564210, 5, Flush);
    PokeBranch(0x4EFC56, 0xE8, CinemaBars, 5);
    // peep: the stolen `call 4F1540` is rebuilt in the trampoline, then back to 4F9BB5
    if (!g_tramp) g_tramp = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (g_tramp && g_trampUsed + 16 <= 4096) {
        t = g_tramp + g_trampUsed;
        t[0] = 0xE8;
        rel = 0x4F1540 - ((DWORD)(ULONG_PTR)t + 5);
        memcpy(t + 1, &rel, 4);
        t[5] = 0xE9;
        rel = 0x4F9BB5 - ((DWORD)(ULONG_PTR)t + 10);
        memcpy(t + 6, &rel, 4);
        g_trampUsed += 16;
        g_peepTramp = (int(__cdecl *)(int))t;
        PokeBranch(0x4F9BB0, 0xE9, Peep, 5);
    }
    g_prisonTramp = (int(__cdecl *)(BYTE *))InlineHook(0x4E8950, 6, PrisonPeep);
    ApplyWindowView43();  // v3.41 VW341
    {  // the mouse
        g_cursorTramp = (void(__cdecl *)(float *, float *))InlineHook(0x405980, 9, MenuCursor);
        g_setCursorTramp = (void(__cdecl *)(double, double))InlineHook(0x4058F0, 9, SetMenuCursor);
        g_hitFTramp = (int(__cdecl *)(float *))InlineHook(0x405760, 9, HitRectF);
        g_hitITramp = (int(__cdecl *)(int *))InlineHook(0x405690, 9, HitRectI);
        g_prevTramp = (int(__cdecl *)(void))InlineHook(0x405570, 9, PrevPage);
        g_nextTramp = (int(__cdecl *)(void))InlineHook(0x405600, 9, NextPage);
    }
    Log("menus 4:3: menus, text, HUD and movies stay 4:3 in wide shapes (world effects full width)");
    ApplyFills();  // v1.6
}

#include "descfit.inc"  // v3.32 DESC332: Options descriptions fit the 4:3 canvas (wrap, else tighter, else shortened)

static void ApplyAspectMenu(void)
{
    static const Site kSites[] = {
        { 0x50C440, { 0x56, 0x8B, 0x74, 0x24, 0x08, 0x85, 0xF6, 0x7D, 0x06 }, 9 },
        { 0x523AA0, { 0x53, 0x56, 0x8B, 0xF0, 0x33, 0xC0, 0x85, 0xD2 }, 8 },
        { 0x5242A0, { 0xA1, 0x20, 0x22, 0x09, 0x01, 0x85, 0xC0, 0x75, 0x09 }, 9 },
        { 0x414917, { 0x83, 0xF9, 0x05, 0x0F, 0x87, 0x89, 0, 0, 0, 0xFF, 0x24, 0x8D, 0xE4, 0x49, 0x41, 0x00 }, 16 },
        { 0x41D600, { 0x83, 0xEC, 0x04, 0xC7, 0x04, 0xE4, 0, 0, 0x48, 0x3F, 0x80, 0x3D, 0xF0, 0x1A, 0x09, 0x01 }, 16 },
        { 0x5A9360, { 0xB8, 0, 0, 0x80, 0x3F, 0x80, 0x3D, 0xF0, 0x1A, 0x09, 0x01, 0x02, 0x72, 0x05, 0xB8 }, 15 },
        { 0x55BDB1, { 0xE9, 0xAA, 0xD5, 0x04, 0x00 }, 5 },  // GOG's jump into 5A9360 (not changed, only checked)
        { 0x407739, { 0xFF, 0x15, 0x18, 0x72, 0x5B, 0x00 }, 6 },
        { 0x405972, { 0xFF, 0x15, 0x20, 0x72, 0x5B, 0x00 }, 6 },
        { 0x5252B0, { 0x80, 0x3D, 0xF0, 0x1A, 0x09, 0x01, 0x02, 0x7E }, 8 },  // the wide-label arrow test (mouse)
        { 0x52587C, { 0x80, 0x3D, 0xF0, 0x1A, 0x09, 0x01, 0x02, 0x7E, 0x14, 0x85, 0xF6, 0x75, 0x10, 0xA1, 0x50, 0x1A, 0x09,
                      0x01, 0x85, 0xC0, 0x75, 0x07, 0x83, 0xC5, 0x14, 0x89, 0x6C, 0x24, 0x38 }, 29 },  // and its draw
        { 0x523DF4, { 0xE8, 0xD7, 0xE7, 0x02, 0x00 }, 5 },  // call 5525D0 (row rectangle)
        { 0x570D11, { 0xE8, 0x1A, 0x39, 0xFB, 0xFF }, 5 },  // load: call 524630 (slot Options -> live)
        { 0x571DED, { 0xE8, 0x3E, 0x28, 0xFB, 0xFF }, 5 },  // load (other path): call 524630
        { 0x525728, { 0x83, 0xF8, 0x05, 0x75 }, 4 },
        { 0x525859, { 0x83, 0xFB, 0x05, 0x75 }, 4 },
        { 0x525292, { 0x83, 0x3D, 0x50, 0x1A, 0x09, 0x01, 0x05, 0x75 }, 8 },
        { 0x525CF0, { 0x83, 0x7B, 0x30, 0x05, 0x75 }, 5 },
        { 0x655034, { 7, 0, 0, 0 }, 4 },
        { 0x655040, { 7, 0, 0, 0 }, 4 },
        { 0x655064, { 6, 0, 0, 0 }, 4 },
        { 0x655058, { 0xC0, 0x4C, 0x65, 0x00 }, 4 },
        { 0x655074, { 0xC0, 0x4C, 0x65, 0x00 }, 4 },
        { 0x655080, { 0xC0, 0x4C, 0x65, 0x00 }, 4 },
        { 0x654CC0, { 0x30, 0x50, 0x52, 0x00, 0xC0, 0x6D, 0x52, 0x00 }, 8 },  // row 0 = Resolution (525030, 526DC0)
    };
    const DWORD *stockRows = (const DWORD *)0x654CC0;
    BYTE zoom[11];
    DWORD rel;
    int i, a, k, bad = 0;
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) {
            const BYTE *p = (const BYTE *)(ULONG_PTR)kSites[i].va;
            Log("aspect menu: unexpected bytes at %08lX (%02X %02X %02X %02X ...)", kSites[i].va, p[0], p[1], p[2], p[3]);
            ++bad;
        }
    if (*(const DWORD *)0x523E64 != 0x654CC0) {
        Log("aspect menu: unexpected imm32 at 00523E64 (%08lX)", *(const DWORD *)0x523E64);
        ++bad;
    }
    if (stockRows[7 * 7] != 0 || stockRows[6 * 7 + 5] != 0x30) {  // terminator; row 6 = Exit (label 30h)
        Log("aspect menu: the Option 1 row table is not the stock one");
        ++bad;
    }
    if (bad) {
        Log("aspect menu: %d site(s) differ from the GOG v7 exe (another mod or another exe?): Aspect Ratio not added", bad);
        return;
    }
    g_dispRow = g_enable && g_fitWrapper;  // v1.5: borderless needs the window and the wrapper answers
    g_monRow = g_dispRow && g_monN >= 2;   // v1.7: a Monitor row under Display Mode when there is a choice
    {   // v1.9: the field-of-view setter must be the stock one (another mod may own it)
        static const Site kFovSite = { 0x4F70A0, { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0xA3, 0x80, 0x5B, 0xFD, 0x00, 0x5D, 0xC3 }, 13 };
        g_fovRow = SiteOk(&kFovSite) ? 2 : 0;  // v3.3: FOV First Person, FOV Third Person
        if (!g_fovRow) Log("field of view: 004F70A0 is not the stock setter (another mod?): no Field of View row");
    }
    // strings
    Encode(g_txtDisp, sizeof g_txtDisp, "Display Mode");
    Encode(g_txtDispDesc, sizeof g_txtDispDesc, "A window, or the whole screen.");
    g_dmShift = 0;
    for (a = 0; a < DM_N; ++a) {
        Encode(g_txtDm[a], sizeof g_txtDm[a], kDmName[a]);
        g_dmIds[a] = MSG_DM0 + a;
        if (ShiftFor((int)strlen(kDmName[a])) > g_dmShift) g_dmShift = ShiftFor((int)strlen(kDmName[a]));
    }
    Encode(g_txtMon, sizeof g_txtMon, "Monitor");
    Encode(g_txtMonDesc, sizeof g_txtMonDesc, "The screen the game is shown on.");
    g_monShift = 0;
    g_monLongW = 0;
    for (a = 0; a < g_monN; ++a) {
        const char *s = MonLabel(a);
        Encode(g_txtMons[a], sizeof g_txtMons[a], s);
        g_monIds[a] = MSG_MON0 + a;
        if (ShiftFor((int)strlen(s)) > g_monShift) g_monShift = ShiftFor((int)strlen(s));
        if (strlen(s) > LONGVAL_CHARS && TextW((int)strlen(s)) > g_monLongW) g_monLongW = TextW((int)strlen(s));  // C2b
    }
    Encode(g_txtAspect, sizeof g_txtAspect, "Aspect Ratio");
    Encode(g_txtAspectDesc, sizeof g_txtAspectDesc, "Sets the shape of the picture.");
    for (a = 0; a < ASP_N; ++a) {
        char s[16];
        Encode(g_txtAsp[a], sizeof g_txtAsp[a], kAspName[a]);
        g_aspIds[a] = MSG_ASP0 + a;
        for (k = 0; k < 6; ++k) {
            sprintf_s(s, sizeof s, "%dx%d", kSizes[a][k][0], kSizes[a][k][1]);
            Encode(g_txtRes[a][k], sizeof g_txtRes[a][k], s);
            g_resLen[a][k] = (BYTE)strlen(s);
        }
    }
    for (k = 0; k < 6; ++k) g_resIds[k] = MSG_RES0 + k;
    Encode(g_txtFov, sizeof g_txtFov, "3rd Person FOV");  // v1.9 Field of View; v3.3 split; v3.5.0 renamed
    Encode(g_txtFovDesc, sizeof g_txtFovDesc, "View angle in the worlds, in degrees.");
    Encode(g_txtFovFp, sizeof g_txtFovFp, "1st Person FOV");
    Encode(g_txtFovFpDesc, sizeof g_txtFovFpDesc, "View angle in Room 302, in degrees.");
    for (a = 0; a < FOV_N; ++a) {
        Encode(g_txtFovs[a], sizeof g_txtFovs[a], kFovName[a]);
        g_fovIds[a] = MSG_FOV0 + a;
    }
    for (a = 0; a < FOVFP_N; ++a) g_fovFpIds[a] = MSG_FOV0 + kFovFpIdx[a];  // v3.3.5: Default, 100..120
    Encode(g_txtFps, sizeof g_txtFps, "Frame Rate");  // v1.9
    Encode(g_txtFpsDesc, sizeof g_txtFpsDesc, g_frameMode ? "Smoother picture. The game stays at 30." : "The game itself runs at this rate.");
    for (a = 0; a < FPS_N; ++a) {
        Encode(g_txtFpss[a], sizeof g_txtFpss[a], kFpsName[a]);
        g_fpsIds[a] = MSG_FPS0 + a;
    }
    {   // v3.14.0: Vibration
        int v;
        for (v = 0; v < 4; ++v) {
            Encode(g_txtVibV[v], sizeof g_txtVibV[v], kVibName[v]);
            g_vibIds[v] = MSG_VIBV0 + v;
        }
        Encode(g_txtVibD, sizeof g_txtVibD, "Off, or how strong. Low is the game's own.");
    }
    {   // v3.13.0: Subtitle Style
        static const char *const kLabel[SUB_N + 1] = { "Size", "Colour", "Edge", "Box", "Effect", "Exit", "Back" };
        static const char *const kDescTxt[SUB_N + 1] = { "Subtitle size. Long lines are fitted.",
                                                         "Subtitle text colour.",
                                                         "Shadow or outline around the letters.",
                                                         "A dark box behind the subtitle.",
                                                         "How a line appears and moves.",
                                                         "How a line leaves.",
                                                         "Back to Option 3." };
        int r, v;
        Encode(g_txtSubT, sizeof g_txtSubT, "Subtitle Style");
        Encode(g_txtSubRowD, sizeof g_txtSubRowD, "Subtitles on or off; Enter opens their style.");  // v3.19.0
        for (r = 0; r <= SUB_N; ++r) {
            Encode(g_txtSubL[r], sizeof g_txtSubL[r], kLabel[r]);
            Encode(g_txtSubD[r], sizeof g_txtSubD[r], kDescTxt[r]);
        }
        for (r = 0; r < SUB_N; ++r)
            for (v = 0; v < kSubCount[r]; ++v) {
                Encode(g_txtSubV[r][v], sizeof g_txtSubV[r][v], kSubName[r][v]);
                g_subIds[r][v] = MSG_SUBV0 + r * SUB_VMAX + v;
            }
    }
    {   // v3.15.0: Screen Effects
        static const char *const kLabel[FX_N + 1] = { "Noise Strength", "Noise Type", "Noise Colour", "Filter", "Cut-scenes", "Back" };  // v3.22.0
        static const char *const kDescTxt[FX_N + 1] = { "Film grain amount. Normal is the game's.",
                                                        "The grain's look. Stock is the game's.",
                                                        "The grain's colour. Stock is each type's own.",
                                                        "A look for the scene. HUD and text stay clean.",
                                                        "The filter in cut-scenes too, or only in play.",
                                                        "Back to Option 3." };
        int r, v;
        Encode(g_txtFxT, sizeof g_txtFxT, "Screen Effects");
        Encode(g_txtFxRowD, sizeof g_txtFxRowD, "Film grain on or off; Enter opens Screen Effects.");  // v3.19.0
        for (r = 0; r <= FX_N; ++r) {
            Encode(g_txtFxL[r], sizeof g_txtFxL[r], kLabel[r]);
            Encode(g_txtFxD[r], sizeof g_txtFxD[r], r == FX_FILTER ? FxFilterDesc() : kDescTxt[r]);  // v3.39 PR339: names PreviewKey
        }
        for (r = 0; r < FX_N; ++r)
            for (v = 0; v < kFxCount[r]; ++v) {
                Encode(g_txtFxV[r][v], sizeof g_txtFxV[r][v], kFxName[r][v]);
                g_fxIds[r][v] = MSG_FXV0 + r * FX_VMAX + v;
            }
    }
    Encode(g_txtItem, sizeof g_txtItem, "Item Shine");  // v3.3; v3.3.5: was "Item Sparkle"
    Encode(g_txtItemDesc, sizeof g_txtItemDesc, "A shine shows where items lie.");
    Encode(g_txtItems[0], sizeof g_txtItems[0], "Off");
    Encode(g_txtItems[1], sizeof g_txtItems[1], "On");
    g_itemIds[0] = MSG_ITEM0;
    g_itemIds[1] = MSG_ITEM0 + 1;
    Encode(g_txtPg[0], sizeof g_txtPg[0], "Option 4 (Presentation)");  // v3.5.0
    Encode(g_txtPg[1], sizeof g_txtPg[1], "Option 5 (Gameplay)");
    Encode(g_txtPg[2], sizeof g_txtPg[2], "Option 6 (Extra Menu)");
    {   // v3.7.0
        static const char *const kEmLabel[EM_N] = { "Storage Chest", "Save Game", "Load Game", "Quick Save", "Quick Load",
                                                     "Eileen Outfit" };
        static const char *const kEmDesc[EM_N] = { "Extra Menu row: the item box anywhere.",
                                                    "Extra Menu row: save anywhere.", "Extra Menu row: load anywhere.",
                                                    "Extra Menu row: a quick save (as the key).",
                                                    "Extra Menu row: the newest quick save.",
                                                    "Extra Menu row: Eileen's outfit." };
        int k;
        for (k = 0; k < EM_N; ++k) {
            Encode(g_txtEm[k], sizeof g_txtEm[k], kEmLabel[k]);
            Encode(g_txtEmD[k], sizeof g_txtEmD[k], kEmDesc[k]);
        }
        Encode(g_txtSb[0], sizeof g_txtSb[0], "Save Banks");  // v3.8.0
        Encode(g_txtSb[1], sizeof g_txtSb[1], "Save Bank");
        Encode(g_txtSbD[0], sizeof g_txtSbD[0], "More save files (Q / E in the save list).");
        Encode(g_txtSbD[1], sizeof g_txtSbD[1], "The bank the save and load lists open on.");
        for (k = 0; k < SB_MAX; ++k) {
            char n[8];
            sprintf_s(n, sizeof n, "%d", k + 1);
            Encode(g_txtSbv[k], sizeof g_txtSbv[k], n);
            g_sbIds[k] = MSG_SBV0 + k;
        }
        Encode(g_txtSbv[SB_MAX], sizeof g_txtSbv[SB_MAX], "-");
        g_sbIds[SB_MAX] = MSG_SBV0 + SB_MAX;
    }
    // v3.5.0 (user, session 9b): Options 1-3 are the stock pages again; Resolution keeps its six sizes per shape (its
    // value labels: row 0's value-table pointer, 0x654CCC, in place). Display Mode, Monitor, Aspect Ratio, 1st / 3rd
    // Person FOV and Frame Rate move to Option 4 (Presentation), Item Shine to Option 5 (Gameplay).
    {
        const DWORD resIds = (DWORD)(ULONG_PTR)g_resIds;
        DWORD *t = g_rowsPres;
        int n = 0;
        if (g_dispRow) {
            g_presDisp = n;
            FillRow(t + 7 * n++, DispInput, DispGet, g_dmIds, DM_N, MSG_DISP, DispDesc);
        }
        if (g_monRow) {  // v1.7
            g_presMon = n;
            FillRow(t + 7 * n++, MonInput, MonRowGet, g_monIds, g_monN, MSG_MON, MonDesc);
        }
        g_presAsp = n;
        FillRow(t + 7 * n++, AspectInput, AspectGet, g_aspIds, ASP_N, MSG_ASPECT, AspectDesc);
        if (g_fovRow) {  // v3.3.5: the first-person row has its own list (Default, 100..120)
            g_presFovFp = n;
            FillRow(t + 7 * n++, FovFpInput, FovFpGet, g_fovFpIds, FOVFP_N, MSG_FOVFP, FovFpDesc);
            g_presFov = n;
            FillRow(t + 7 * n++, FovInput, FovGet, g_fovIds, FOV_N, MSG_FOV, FovDesc);
        }
        if (g_fpsRow) {
            g_presFps = n;
            FillRow(t + 7 * n++, FpsInput, FpsGet, g_fpsIds, FPS_SHOWN, MSG_FPS, FpsDesc);  // v3.31 FPS331: 30 / 60 unless HighFrameRates=1
        }
        n = PresFillRows(t, n);  // v3.39 PR339: Renderer, Opening (were V-Sync, Anti-Aliasing, Anisotropic Filtering, Performance)
        memcpy(t + 7 * n++, stockRows + 6 * 7, 7 * 4);  // Exit (Option 1's record)
        t[7 * n] = 0;
        g_lpTable[3] = t;
        g_lpCount[3] = n;
        g_lpTitle[3] = MSG_PG4;
        FillRow(g_rowShine, ItemInput, ItemGet, g_itemIds, 2, MSG_ITEM, ItemDesc);  // v3.37 MN337: laid out by GpBuildPages
        memcpy(g_rowsPlay, g_rowShine, 7 * 4);  // (alone with Exit without the folders)
        memcpy(g_rowsPlay + 7, stockRows + 6 * 7, 7 * 4);
        g_rowsPlay[14] = 0;
        g_playShineRow = 0;  // v3.37 MN337
        g_lpTable[4] = g_rowsPlay;
        g_lpCount[4] = 2;
        {   // v3.25.0: + Player / Enemies (hidden pages), when Option 2's Button Config record is the stock one (their shape)
            static const DWORD kBc[7] = { 0x524940, 0x51EB10, 0, 0x654C54, 1, 0x27, 0x523130 };
            if (!memcmp((const void *)0x654D88, kBc, sizeof kBc)) {
                g_lpCount[4] = GpBuildPages(stockRows, (const DWORD *)0x654D88, g_rowShine, g_rowsPlay, g_rowsPlayer, g_rowsEnemy,
                                            g_rowsSpecial);  // v3.27.0: + Special; v3.37 MN337: 9 rows, Item Shine 5th
                g_gpMenu = 1;
            } else {
                Log("gameplay: Option 2's Button Config record is not the stock one: no Player / Enemies pages ([Gameplay] in the ini)");
            }
        }
        g_lpTitle[4] = MSG_PG5;
        {   // v3.7.0: Option 6 (Extra Menu): one Off / On row per pause Extra Menu row, then Exit
            static const void *const kIn[EM_N] = { EmIn0, EmIn1, EmIn2, EmIn3, EmIn4, EmIn5 };
            static const void *const kGet[EM_N] = { EmGet0, EmGet1, EmGet2, EmGet3, EmGet4, EmGet5 };
            static const void *const kDesc[EM_N] = { EmDesc0, EmDesc1, EmDesc2, EmDesc3, EmDesc4, EmDesc5 };
            int k;
            for (k = 0; k < EM_N; ++k) FillRow(g_rowsExtra + 7 * k, kIn[k], kGet[k], g_itemIds, 2, MSG_EM0 + k, kDesc[k]);
            // v3.8.0: Save Banks (Off / On) and Save Bank (1-99)
            FillRow(g_rowsExtra + 7 * EM_N, SbOnIn, SbOnGet, g_itemIds, 2, MSG_SB, SbOnDesc);
            // v3.33 CAM333: Camera replaces Save Bank (the bank the lists open on: Q / E in the save and load lists, [SaveBanks] Bank)
            CamFillRow(g_rowsExtra + 7 * (EM_N + 1));
            memcpy(g_rowsExtra + 7 * (EM_N + 2), stockRows + 6 * 7, 7 * 4);
            g_rowsExtra[7 * (EM_N + 3)] = 0;
        }
        g_lpTable[5] = g_rowsExtra;
        g_lpCount[5] = EM_N + 3;
        g_lpTitle[5] = MSG_PG6;
        // (v3.37 CP337: no Camera page in Options any more: its settings are the pause menu's Extra Menu > Camera)
        {   // v3.13.0: the Subtitle Style page (hidden, LP_SUB): Size, Colour, Edge, Box, Back
            static const void *const kIn[SUB_N] = { SubIn0, SubIn1, SubIn2, SubIn3, SubIn4, SubIn5 };
            static const void *const kGet[SUB_N] = { SubGet0, SubGet1, SubGet2, SubGet3, SubGet4, SubGet5 };
            static const void *const kDesc[SUB_N] = { SubDesc0, SubDesc1, SubDesc2, SubDesc3, SubDesc4, SubDesc5 };
            int k;
            for (k = 0; k < SUB_N; ++k) FillRow(g_rowsSub + 7 * k, kIn[k], kGet[k], g_subIds[k], kSubCount[k], MSG_SUBL0 + k, kDesc[k]);
            memcpy(g_rowsSub + 7 * SUB_N, stockRows + 6 * 7, 7 * 4);  // Exit's record with our input / label / description
            g_rowsSub[7 * SUB_N + 0] = (DWORD)(ULONG_PTR)SubBackIn;
            g_rowsSub[7 * SUB_N + 5] = MSG_SUBL0 + SUB_N;
            g_rowsSub[7 * SUB_N + 6] = (DWORD)(ULONG_PTR)SubDescBack;
            g_rowsSub[7 * (SUB_N + 1)] = 0;
            g_lpTable[LP_SUB] = g_rowsSub;
            g_lpCount[LP_SUB] = SUB_N + 1;
            g_lpTitle[LP_SUB] = MSG_SUBT;
        }
        {   // v3.15.0: the Screen Effects page (hidden, LP_FX): Noise Strength, Noise Type (v3.16.0), Filter, Back
            static const void *const kIn[FX_N] = { FxIn0, FxIn1, FxIn2, FxIn3, FxIn4 };
            static const void *const kGet[FX_N] = { FxGet0, FxGet1, FxGet2, FxGet3, FxGet4 };
            static const void *const kDesc[FX_N] = { FxDesc0, FxDesc1, FxDesc2, FxDesc3, FxDesc4 };
            int k, nb;  // v3.43 PP343: nb = the Back row (after our post rows)
            for (k = 0; k < FX_N; ++k) FillRow(g_rowsFx + 7 * k, kIn[k], kGet[k], g_fxIds[k], kFxCount[k], MSG_FXL0 + k, kDesc[k]);
            nb = FX_N + PpBuildRows(g_rowsFx + 7 * FX_N);  // v3.43 PP343: Bloom, Colour Grading, Otherworld Lens (0 rows when off)
            nb += DofBuildRows(g_rowsFx + 7 * nb, nb);  // v3.44 DF344: Depth of Field, DoF Strength (0 rows when it is not installed)
            memcpy(g_rowsFx + 7 * nb, stockRows + 6 * 7, 7 * 4);  // Exit's record with our input / label / description
            g_rowsFx[7 * nb + 0] = (DWORD)(ULONG_PTR)FxBackIn;
            g_rowsFx[7 * nb + 5] = MSG_FXL0 + FX_N;
            g_rowsFx[7 * nb + 6] = (DWORD)(ULONG_PTR)FxDescBack;
            g_rowsFx[7 * (nb + 1)] = 0;
            g_lpTable[LP_FX] = g_rowsFx;
            g_lpCount[LP_FX] = nb + 1;
            g_lpTitle[LP_FX] = MSG_FXT;
        }
        ShBuildPage(stockRows);  // v3.40 SH340: the Shadows page (hidden, LP_SHD): Shadow Edge, Back
        DofBuildPage(stockRows);  // v3.43 DF343: the Depth of Field page (hidden, LP_DOF): Depth of Field, Strength, Back
        if (!InstallPages()) {
            Log("options: Display Mode, Monitor, Aspect Ratio, FOV, Frame Rate and Item Shine are set in SH4Refined.ini only");
            g_bldRowOn = 0;  // review REV-325-009: the stock Option 3 table is in use: the saved colour stays 0..2
        }
        if (ShMenuOn() && !g_subScreenOn) ShMenuOff("the Options sub-screen fade is not in place");  // v3.40 SH340
        if (DofMenuOn() && !g_subScreenOn) DofMenuOff("the Options sub-screen fade is not in place");  // v3.43 DF343
        if (g_gpMenu && !g_subScreenOn) {  // v3.25.0: the pages need the fade route; without it Option 5 is Item Shine + Exit
            g_lpCount[4] = GpNoFadeLayout(stockRows, g_rowsPlay);  // v3.37 MN337: Item Shine moved from its row 4 to row 0
            Log("gameplay: no sub-screen fade: no Player / Enemies pages ([Gameplay] in the ini still applies)");
        }
        if (*(const DWORD *)0x654CCC == 0x654C9C)  // Resolution's value labels (data; review OPT-REV-002: stock first)
            Poke(0x654CCC, &resIds, 4);
        else
            Log("options: Resolution's value table 00654CCC is %08lX, not the stock 00654C9C (another mod?): its labels stay",
                *(const DWORD *)0x654CCC);
        FnRowInstall();  // v3.44 FN344: Option 1 gets the Menu Font row (a copy of its table, after the labels above)
        OmRowInstall();  // v3.47 CT347: Option 2 gets the Stick Movement row (a copy of its table, after InstallPages' Vibration copy)
    }
    PokeBranch(0x50C440, 0xE9, hkMsgLookup, 7);
    PokeBranch(0x523AA0, 0xE9, hkRowChanged, 6);
    {   // v3.22.0 (items 15 / 16): the submenu rows' colour cue and the Screen Effects text outline (each site optional)
        static const Site kLbl = { 0x525553, { 0xE8, 0x48, 0xE4, 0xFF, 0xFF }, 5 };
        static const Site kSh[3] = { { 0x523A3A, { 0xE8, 0x41, 0x7B, 0xFE, 0xFF }, 5 }, { 0x52418D, { 0xE8, 0xEE, 0x73, 0xFE, 0xFF }, 5 },
                                     { 0x5259C3, { 0xE8, 0xB8, 0x5B, 0xFE, 0xFF }, 5 } };
        int k, nsh = 0, descSh = 0;
        const int lbl = SiteOk(&kLbl) && PokeBranch(0x525553, 0xE8, hkRowLabel, 5);
        const int desc = DescSitesOk();  // v3.32 DESC332: the description shadow goes through DescShadow (-> OutlineShadow)
        for (k = 0; k < 3; ++k)
            if (SiteOk(&kSh[k]) && PokeBranch(kSh[k].va, 0xE8, k == 1 && desc ? (void *)DescShadow : (void *)OutlineShadow, 5)) {
                ++nsh;
                if (k == 1 && desc) descSh = 1;  // v3.32 DESC332
            }
        DescInstall(desc, descSh);  // v3.32 DESC332
        if (!lbl) g_subArrows = 1;  // no colour cue: keep the v3.19 arrows
        {   // v3.33 MERGE: a value's colour (Fly greyed while no over-the-shoulder camera is on); optional, alone
            static const Site kVal = { 0x525802, { 0xE8, 0x99, 0xE1, 0xFF, 0xFF }, 5 };
            if (SiteOk(&kVal) && PokeBranch(0x525802, 0xE8, hkRowValue, 5))
                Log("options: greyed values drawn grey (0x525802)");
            else
                Log("options: 0x525802 is not stock: No Clip's Fly is not drawn grey (the row still refuses it without a camera)");
        }
        Log("options: submenu rows %s; Screen Effects text outline at %d of 3 shadow sites", lbl ? "blue (0x525553)" : "keep their arrows (0x525553 not stock)", nsh);
    }
    PokeBranch(0x5242A0, 0xE9, SetResIndex, 5);
    // the wide-label arrow (v1.5: follows Resolution to row 1, Display Mode's arrow; v1.6: by label length, always)
    PokeBranch(0x5252B0, 0xE9, ArrowShifted, 5);
    PokeBranch(0x52587C, 0xE9, hkArrowDraw, 29);
    {   // v3.19.0 C2b: long values (Monitor rank names, Night Vision Mono): the column moves, both arrows follow
        static const Site kV = { 0x52570E, { 0xA0, 0xDC, 0x1A, 0x09, 0x01 }, 5 };
        static const Site kM = { 0x525CE3, { 0x80, 0x3D, 0xDC, 0x1A, 0x09, 0x01, 0x02 }, 7 };
        if (SiteOk(&kV) && SiteOk(&kM) && PokeBranch(0x52570E, 0xE9, hkValueCol, 5)) {
            if (PokeBranch(0x525CE3, 0xE9, hkMouseCol, 7)) {
                Log("options: long values move their column (0052570E draw, 00525CE3 mouse)");
            } else if (!Poke(0x52570E, kV.stock, 5)) {
                Log("options: 0052570E could not be put back");
            }
        } else {
            Log("options: 0052570E / 00525CE3 are not stock: long values keep the right-arrow shift only");
        }
    }
    if (g_fovRow) {  // v1.9
        SetFov(FovIndexOf(g_fovIni), 0, 0);  // v2.0: degrees
        SetFov(FovIndexOf(FovFpAllowedDeg(g_fovFpIni)), 0, 1);  // v3.3; v3.3.5: Default or 100..120 only
        PokeBranch(0x4F70A0, 0xE9, hkFovSet, 5);
        Log("field of view: rows under Aspect Ratio, first person %s%s, third person %s%s; setter 004F70A0 hooked",
            kFovName[g_fovFpSel], g_fovFpSel ? " degrees" : "", kFovName[g_fovSel], g_fovSel ? " degrees" : "");
    }
    PokeBranch(0x570D11, 0xE8, LoadOptions, 5);  // v1.4.1: a load keeps the current shape
    PokeBranch(0x571DED, 0xE8, LoadOptions, 5);
    PokeBranch(0x414917, 0xE9, hkSizePick, 9);  // the rest of the old jump stays unreachable
    PokeBranch(0x41D600, 0xE9, CamAspect, 5);
    // 5A9360: call CamZoomBits / push eax / jmp 55BDB6
    zoom[0] = 0xE8;
    rel = (DWORD)(ULONG_PTR)CamZoomBits - (0x5A9360 + 5);
    memcpy(zoom + 1, &rel, 4);
    zoom[5] = 0x50;
    zoom[6] = 0xE9;
    rel = 0x55BDB6 - (0x5A9366 + 5);
    memcpy(zoom + 7, &rel, 4);
    Poke(0x5A9360, zoom, 11);
    if (g_enable && g_fitWrapper) {  // the wrapper then removes only the client origin (scale 1), see MapCursor
        PokeBranch(0x407739, 0xE8, GameGetCursorPos, 6);
        PokeBranch(0x405972, 0xE8, GameSetCursorPos, 6);
    }
    if (g_pagesOn)
        Log("options: Option 4 (Presentation) has %s%sAspect Ratio (4:3, 16:10, 16:9, 21:9, 32:9)%s%s, Option 5 (Gameplay) "
            "Item Shine; Options 1-3 are stock (Resolution follows Aspect Ratio); %d pages", g_dispRow ? "Display Mode "
            "(Windowed, Borderless; Alt+Enter), " : "", g_monRow ? "Monitor, " : "", g_fovRow ? ", 1st / 3rd Person FOV" : "",
            g_fpsRow ? ", Frame Rate" : "", LP_N);
    if (g_menus43Ini) ApplyMenus43();  // v1.4
}

// ================================================================ FRAME INTERPOLATION (v2.1 prototype): host glue
// The module itself is interp.c (no game addresses in there); this part tells it the game's state. FrameMode=1: the
// governor keeps the logic at 30 and Governor() hands the device to Interp_FrameEnd every frame.
static void InterpLog(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsprintf_s(buf, sizeof buf, fmt, ap);
    va_end(ap);
    Log("%s", buf);
}

// the governor's gameplay test: mode 6, not an in-game menu (overlay 2/2 or game phase 2)
static int InterpGameplay(void)
{
    const int mode = *(volatile int *)0x1236D4C;
    return mode == 6 && !((*(volatile int *)0x1236D50 == 2 && *(volatile int *)0x1236D54 == 2) || *(volatile int *)0xFD5C20 == 2)
        && (g_smoothCut || *(volatile int *)0xFD5C20 != 1);  // v2.1: real-time cutscenes (phase 1) at 30 unless SmoothCutscenes=1
}

// main loop 0x414FEF..0x41526E: [0x13DC390] = QPC at this tick's start, [0xECB330] = its deadline, step [0x610DDC];
// after the frame the loop sets deadline += step (frame start + step if that is behind) and spins until it passes.
static int InterpTick(unsigned __int64 *frameStart, unsigned __int64 *nextTick, unsigned __int64 *step)
{
    unsigned __int64 d, n;
    if (!*(volatile int *)0xECB328) return 0;  // timeGetTime fallback (no QPC)
    *frameStart = *(volatile unsigned __int64 *)0x13DC390;
    d = *(volatile unsigned __int64 *)0xECB330;
    *step = *(volatile DWORD *)0x610DDC;
    n = d + *step;
    if (n < *frameStart) n = *frameStart + *step;
    *nextTick = n;
    return 1;
}

static unsigned InterpTickId(void) { return *(volatile DWORD *)0x13DC324; }  // [0x13DC324]++ once per tick (0x42D1B4)

static int MonitorRefresh(void)
{
    MONITORINFOEXA mi;
    DEVMODEA dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    MonGet((int)g_monSel, &mi);
    if (EnumDisplaySettingsA(mi.szDevice[0] ? mi.szDevice : NULL, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
        return (int)dm.dmDisplayFrequency;
    return 0;
}

// ---- v2.2: view-culling margin for the in-between pictures (research\framerate\v4\culling.md)
// An in-between picture replays tick T's draw list with the camera blended back toward T-1, so whatever the game culled
// at T because it was just outside the view is missing at the edges of that picture while the camera turns (the user's
// "left/right edges of the screen glitch a little" in Room 302). The game culls models (0x4280E0, outcode 0x428060,
// called once at 0x42814B) and map pieces (0x422E40, matrix copied to 0xED8450 at 0x422B40, read only by the cull) against
// the tick's own frustum. Both tests get a margin: x against kx|w|, y against ky|w|; z (near / far) unchanged, nothing
// drawn changes shape, a few more objects just outside the view are drawn. [Window] InterpCullMargin (not in the default
// ini): percent wider horizontally (vertically half of it), default 25, 0 = off.
static float g_cullKx = 1.25f, g_cullKy = 1.125f;

static DWORD __fastcall ModelOutcode(void *unused, const float *c)  // replaces call 0x428060 at 0x42814B (edx = clip)
{
    const float w = fabsf(c[3]), wx = w * g_cullKx, wy = w * g_cullKy;
    DWORD b = 0;
    (void)unused;
    if (c[0] >= wx) b |= 0x01;
    if (c[0] <= -wx) b |= 0x02;
    if (c[1] >= wy) b |= 0x04;
    if (c[1] <= -wy) b |= 0x08;
    if (c[2] >= w) b |= 0x10;
    if (c[2] <= -w) b |= 0x20;
    return b;
}

static void __cdecl MapCullMatrix(float *dst, const float *src)  // replaces call 0x421710 at 0x422B40 (dst = 0xED8450)
{
    int i;
    memcpy(dst, src, 64);
    for (i = 0; i < 4; ++i) {  // row-vector convention: clip.x = column 0, clip.y = column 1
        dst[i * 4 + 0] /= g_cullKx;
        dst[i * 4 + 1] /= g_cullKy;
    }
}

static void CullMarginInstall(void)
{
    static const Site kModel = { 0x42814B, { 0xE8, 0x10, 0xFF, 0xFF, 0xFF }, 5 };
    static const Site kMap = { 0x422B40, { 0xE8, 0xCB, 0xEB, 0xFF, 0xFF }, 5 };
    int pct = (int)GetPrivateProfileIntA("Window", "InterpCullMargin", 25, g_iniPath), n = 0;
    if (pct <= 0) {
        Log("interp: InterpCullMargin=0: the game culls at the tick's own view (edges may miss objects while the camera turns)");
        return;
    }
    if (pct > 60) pct = 60;
    g_cullKx = 1.0f + pct / 100.0f;
    g_cullKy = 1.0f + pct / 200.0f;
    if (SiteOk(&kModel)) n += PokeBranch(0x42814B, 0xE8, ModelOutcode, 5);
    else Log("interp: 0042814B is not stock: models are culled at the tick's own view");
    if (SiteOk(&kMap)) n += PokeBranch(0x422B40, 0xE8, MapCullMatrix, 5);
    else Log("interp: 00422B40 is not stock: map pieces are culled at the tick's own view");
    Log("interp: view culling %d%% wider sideways, %d%% up and down, for the in-between pictures (%d of 2 sites)", pct, pct / 2, n);
}

static void InterpStart(int displayRate)  // DllMain (ApplyFrameRate), FrameMode=1
{
    static const InterpHost host = { InterpLog, InterpGameplay, InterpTick, InterpTickId };
    Interp_SetAutoLower(g_interpAutoLower);
    CullMarginInstall();
    if (!GetPrivateProfileIntA("Window", "InterpMatrixRule", 1, g_iniPath)) {  // v2.2: 0 = v2.1 per-row jump rule
        Interp_SetMatrixRule(0);
        Log("interp: InterpMatrixRule=0: matrices judged row by row (v2.1)");
    }
    if (!GetPrivateProfileIntA("Window", "InterpPacing", 1, g_iniPath)) {  // v2.2: 0 = the v2.1 spacing rule
        Interp_SetPacing(1);
        Log("interp: InterpPacing=0: the v2.1 spacing rule (pictures share the rest of the tick by k)");
    }
    {   // v2.2 research aid, not in the default ini: 1 = pacing file + frame dumps, 2 = frame dumps only (Ctrl+F12)
        const int diag = (int)GetPrivateProfileIntA("Window", "InterpDiag", 0, g_iniPath);
        if (diag == 1 || diag == 2) {
            Interp_SetDiag(diag, g_dir);
            Log("interp: InterpDiag=%d: %sframe dumps on Ctrl+F12 (or SH4Refined_dump.flag)", diag,
                diag == 1 ? "SH4Refined_pacing.csv and " : "");
        }
    }
    Interp_Init(&host, displayRate, 0);  // v3.12.0 (review GFX-003): displayRate is already V-Sync capped (FpsCapUpdate)
    if (!g_interpAutoLower) Log("interp: InterpAutoLower=0: the display rate is never lowered automatically");
}

// ================================================================ FRAME RATE ENGINE (v2.0)
// v1.9 set the rate once in DllMain (next start only) and scaled the animation rate, so every menu, the title and the
// cutscenes ran N/30 times too fast. v2.0 (research fps_title_live.md, 2026-11-03):
// * A governor at the end of every frame (0x415202, the call of 0x55ABF0 = EndScene / Present) runs the game at 30 on
//   the front end (boot screens, title, Load, Options: mode [0x1236D4C] != 6), in the in-game menus ([0x1236D50] == 2
//   && [0x1236D54] == 2: map, items, memos, pause, Options; the world is frozen), and at the chosen rate in gameplay.
//   v2.1: real-time cutscenes (game phase [0xFD5C20] == 1) run at 30 too; in that phase, as in the menus' phase 2,
//   the room objects (hauntings included) and enemies do not run, so no tick counter of theirs sees the switch; the
//   phase setters switch in the tick of the change. The engine reads its rate [0x5D5248] (0x55AE50 / 0x55AE60) and the loop step
//   [0x610DDC] (QPF / rate, read every frame at 0x41521C) live, so a switch at a frame boundary is clean, and a change
//   in the menu applies when play resumes.
// * Animations: each anim caches its per-tick speed (+0x38, from [0xED57DC] = 7680 / 30 = 256); the three advance sites
//   (0x4184DC, 0x4192B5, 0x41848E) scale it by 30 / [0x5D5248] here, with the global tick counter spreading the
//   remainder so the average speed is exact. [0xED57DC] stays at its 30 Hz value.
// * Per-tick constants of the exe (research fps_tail.md: 164 sites, kTail, and the key repeat, kRateImm8), rewritten at
//   every switch; the fade speed floor (B1) and the monsters' spring helper (B2). The imm32 of `mov [5D5248], imm` at
//   0x403169 holds the chosen gameplay rate: Playable Eileen's GameRate() reads it (KB 58).
static volatile LONG g_fpsUser = 30;  // the gameplay rate chosen (ini / menu)
static int g_fpsCap;                  // full screen: the monitor's refresh, when VSync would hold the game below the rate
static int g_rateNow = 30;            // the rate in force ([0x5D5248] as this mod last wrote it)
static int g_rateHooks;               // the frame-rate hooks are in place

// Writes code bytes again at run time (not listed again in SH4WM_Patches: Poke listed the site once)
static void RateWrite(DWORD va, const void *bytes, int len)
{
    BYTE *p = (BYTE *)(ULONG_PTR)va;
    DWORD old;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &old)) return;
    memcpy(p, bytes, len);
    VirtualProtect(p, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
}

// imm8 operands set from the rate. value(rate): ticks scaled by rate/30 (a time), or the menu key-repeat mask.
enum { RI_TICKS, RI_PER_TICK, RI_REPEAT_MASK };
static const struct { DWORD site; BYTE stock[3]; DWORD imm; int kind, base; const char *what; } kRateImm8[] = {
    { 0x4F85E3, { 0x83, 0xFA, 0x10 }, 0x4F85E5, RI_TICKS, 16, "menu / item belt key repeat: delay" },
    { 0x4F85EA, { 0x83, 0xE1, 0x01 }, 0x4F85EC, RI_REPEAT_MASK, 1, "menu / item belt key repeat: every 2nd tick" },
};
static int g_rateImmOk[sizeof kRateImm8 / sizeof kRateImm8[0]];

static BYTE RateImm8(int kind, int base, int rate)
{
    int v;
    if (kind == RI_TICKS) v = (base * rate + 15) / 30;
    else if (kind == RI_PER_TICK) v = (base * 30 + rate / 2) / rate;
    else v = rate >= 180 ? 15 : rate >= 120 ? 7 : rate >= 60 ? 3 : base;  // power of 2 minus 1 nearest 2 * rate / 30
    if (v < 1) v = 1;
    if (v > 127) v = 127;
    return (BYTE)v;
}

static void TailApply(int rate);
static void TailInstall(void);
static int g_tailInstalled;
static void GateAdvance(int rate);  // v2.1 ENTITY TICK GATE (defined after the tail)
static int g_inGate;  // v2.1 ENTITY TICK GATE: inside a gated (30 Hz) entity update (tentative definition)

static void ApplyRate(int rate)
{
    int i;
    const int tick = g_rateProbe ? 30 : rate;  // v3.0 RateProbe: the loop's real rate stays 30
    *(volatile int *)0x5D5248 = rate;
    if (*(int *)0xECB328) {  // QPC works: the step is QPF / rate (64-bit QPF at 0x13DC398)
        const unsigned __int64 qpf = *(volatile unsigned __int64 *)0x13DC398;
        if (qpf) *(volatile DWORD *)0x610DDC = (DWORD)(qpf / (unsigned)tick);
    } else {
        *(volatile DWORD *)0x610DDC = (1000 + tick / 2) / (DWORD)tick;
    }
    for (i = 0; i < (int)(sizeof kRateImm8 / sizeof kRateImm8[0]); ++i)
        if (g_rateImmOk[i]) {
            const BYTE b = RateImm8(kRateImm8[i].kind, kRateImm8[i].base, rate);
            RateWrite(kRateImm8[i].imm, &b, 1);
        }
    TailApply(rate);
    g_rateNow = rate;
}

// ---- v2.1 MENUS/CUTSCENES (research\framerate\v3\menus_cutscenes_poi.md) ----
// Game phase [0xFD5C20] (main-loop task list 0x62BD00[phase]): 0 = gameplay (worlds AND Room 302, list 0x62BC78),
// 1 = cutscene (list 0x62BCB8: only task groups 0x20 / 0x40 run, the player, enemies and room objects are frozen),
// 2 = overlay (list 0x62BCF0: group 0x40 only; pause menu, item chest, message boxes, map, memos, save screens).
// Only three functions write it: 0x4EFB30 (= 1, cutscene start, via 0x4CCBF0 from every story / peephole / scripted
// scene handler), 0x4EFAE0 (= 0, scene end via 0x4CCC00, overlay close 0x4F9166, 0x4F73AB) and 0x4EFB70 (= 2, overlay
// open, 0x4F9136 only); 0x4151B7 zeroes it on quit to title. Pre-rendered FMV (0x5211A0 -> 0x412EF0) is a blocking
// loop paced by the movie clock: no frame end runs while it plays, the rate needs nothing there.
static const int g_fpsCut30 = 1;  // v2.1: cutscenes (phase 1) at the stock 30 (the user's request)

static void __cdecl Governor(void)
{
    const int mode = *(volatile int *)0x1236D4C;
    const int phase = *(volatile int *)0xFD5C20;
    // in-game menus: the overlay sub-mode ([0x1236D54] == 2) or game phase 2 ([0xFD5C20]: pause menu, the plugin's
    // menu, map, memos, save screens, the item chest, message boxes; only task group 0x40 runs, the world is frozen)
    const int menu = mode == 6 && ((*(volatile int *)0x1236D50 == 2 && *(volatile int *)0x1236D54 == 2) || phase == 2);
    const int cut = mode == 6 && g_fpsCut30 && phase == 1;  // v2.1: real-time cutscenes
    int want = g_fpsUser;
    static int s_logged;
    if (g_fpsCap && want > g_fpsCap) want = g_fpsCap;
    if (mode != 6 || menu || cut) want = 30;
    if (g_frameMode) {  // v2.1: interpolation; the logic stays at 30
        want = 30;
        Interp_FrameEnd(DEV);
    }
    if (!g_tailInstalled && !g_frameMode) {  // the first frame end: every dll's DllMain has run (v2.1: not with FrameMode=1)
        TailInstall();
        g_rateNow = -1;
    }
    if (want == g_rateNow) return;
    ApplyRate(want);
    if (s_logged < 40) {  // the first switches (menus switch often)
        ++s_logged;
        Log("frame rate: %d (%s)", want, mode != 6 ? "front end" : menu ? "in-game menu" : cut ? "cutscene" : "gameplay");
    }
}

// v2.1: the phase setters run the governor at once, in the tick of the change, instead of at that frame's end. The
// code that runs after a phase change in the same tick sizes its fades from the rate (`0x80 / 0x55AE50()`): the item
// chest's fade-in (0x4CE1EA, `2 * 0x80 / fps`, first tick of the overlay callback 0x4CE1A0) was sized for the gameplay
// rate and then ran at 30: 2x / 4x / 8x too long at 60 / 120 / 180 (the "slow chest"); the fade-in after a cutscene
// or a closed overlay (0x4D08CE, 0x4CD4FA ...) was sized for 30 and ran at the gameplay rate (too short).
static void __cdecl GovernorNow(void)
{
    if (g_tailInstalled && !g_inGate && !g_frameMode) Governor();  // a phase set inside a gated entity update: the frame end decides
}
static const DWORD g_phaseTail = 0x553CF0;
static __declspec(naked) void hkPhase2(void)  // 4EFB70: was `mov dword ptr [FD5C20h], 2; ret` (11 bytes)
{
    __asm {
        mov dword ptr ds:[0FD5C20h], 2
        pushad
        pushfd
        call GovernorNow
        popfd
        popad
        ret
    }
}
static __declspec(naked) void hkPhaseTail(void)  // 4EFB1E (phase 0) / 4EFB69 (phase 1): was `jmp 553CF0`
{
    __asm {
        pushad
        pushfd
        call GovernorNow
        popfd
        popad
        jmp dword ptr [g_phaseTail]
    }
}
// ---- end v2.1 MENUS/CUTSCENES ----

static const DWORD g_presentFn = 0x55ABF0;
static void SbLogTick(void);  // v3.48 SB348: savebanks.inc (the settled bank change's line)
// v2.1: the frame end runs the governor, then the entity gate's 30 Hz accumulator (once per tick; the phase-setter
// hooks call Governor too, so the gate must not advance there)
static void __cdecl FrameEndC(void)
{
    Governor();
    PerfFrame(0);  // v3.37 CP337: the performance overlay's frame clock (perfhud.inc), after the governor
    SbLogTick();   // v3.48 SB348: a bank change in the save / load list settled 1 s ago: its one line
    GateAdvance(*(volatile int *)0x5D5248);  // v2.1 ENTITY TICK GATE: is the next tick a 30 Hz one
}
static __declspec(naked) void hkFrameEnd(void)  // 415202: was `call 55ABF0` (fade quad, EndScene, Present)
{
    __asm {
        pushad
        pushfd
        call FrameEndC
        popfd
        popad
        jmp dword ptr [g_presentFn]
    }
}

// The per-tick advance of an animation (its cached speed, or a delta made from it) at the rate in force: the 30 Hz
// value times 30 / rate, the remainder spread by the global tick counter [0x13DC324] so the average is exact.
static int __cdecl AnimStep(int v)
{
    const int rate = *(volatile int *)0x5D5248;
    __int64 a, t;
    if (rate == 30 || rate < 10 || rate > 1000) return v;
    a = (__int64)v * 30;
    t = (__int64)(*(volatile DWORD *)0x13DC324 % (DWORD)rate);
    // floor(a * (t + 1) / rate) - floor(a * t / rate), floors done for either sign
    {
        __int64 n1 = a * (t + 1), n0 = a * t, q1, q0;
        q1 = n1 >= 0 ? n1 / rate : -((-n1 + rate - 1) / rate);
        q0 = n0 >= 0 ? n0 / rate : -((-n0 + rate - 1) / rate);
        return (int)(q1 - q0);
    }
}

static const DWORD g_animJz1 = 0x418508, g_animBack1 = 0x4184E1, g_animJz2 = 0x4192E1, g_animBack2 = 0x4192BA,
                   g_animBack3 = 0x418495;
static __declspec(naked) void hkAnim1(void)  // 4184DC: was `mov eax,[esi+38h]; je 418508` (flags from cmp [esi],0)
{
    __asm {
        je skip
        push ecx
        push edx
        push dword ptr [esi + 38h]
        call AnimStep
        add esp, 4
        pop edx
        pop ecx
        jmp dword ptr [g_animBack1]
    skip:
        jmp dword ptr [g_animJz1]
    }
}
static __declspec(naked) void hkAnim2(void)  // 4192B5: was `mov eax,[esi+38h]; je 4192E1`
{
    __asm {
        je skip
        push ecx
        push edx
        push dword ptr [esi + 38h]
        call AnimStep
        add esp, 4
        pop edx
        pop ecx
        jmp dword ptr [g_animBack2]
    skip:
        jmp dword ptr [g_animJz2]
    }
}
static __declspec(naked) void hkAnim3(void)  // 41848E: was `mov eax,[esp+0Ch]; mov edx,[esi+34h]` (delta = arg 2)
{
    __asm {
        push ecx
        push dword ptr [esp + 10h]  // the site's [esp+0Ch] (reached by jmp: no return address), + the push above
        call AnimStep
        add esp, 4
        pop ecx
        mov edx, dword ptr [esi + 34h]
        jmp dword ptr [g_animBack3]
    }
}

// (the main loop's QPC step store at 414F0E: QPF / 30 -> QPF / the rate in force)
static DWORD __cdecl FpsTicks(DWORD perFrame30)
{
    const int rate = g_rateProbe ? 30 : *(volatile int *)0x5D5248;  // v3.0 RateProbe: the loop stays at 30
    return rate >= 10 && rate <= 1000 ? (DWORD)((unsigned __int64)perFrame30 * 30 / (DWORD)rate) : perFrame30;
}
static __declspec(naked) void hkFpsStore(void)  // 414F0E: was `mov [610DDCh], eax` (eax = QPF / 30)
{
    __asm {
        push eax
        push ecx
        push edx
        push eax
        call FpsTicks
        add esp, 4
        mov dword ptr ds:[610DDCh], eax
        pop edx
        pop ecx
        pop eax
        ret
    }
}

// ---- the gameplay tail (research fps_tail.md / .json, group A): per-tick constants of the exe, set for the rate in
// force at every switch (30 = the stock bytes). RT_PTR: the instruction reads a constant shared by other code: its
// disp32 points at a float of this dll instead (the constant itself is never changed).
enum { RT_F32, RT_I8, RT_I32, RT_PTR, RT_I16 };  // v3.0: RT_I16 (word operands of the companion / event audit)
typedef struct { DWORD va; int off, type, len; BYTE orig[16]; double v[3]; int group; } RateTail;
typedef struct { DWORD va; int len; BYTE orig[8]; BYTE per[3][8]; } RateRaw;
#include "fps_tail_table.h"
#define NTAIL ((int)(sizeof kTail / sizeof kTail[0]))
#define NTAILRAW ((int)(sizeof kTailRaw / sizeof kTailRaw[0]))
static BYTE g_tailOk[NTAIL], g_tailRawOk[NTAILRAW];
static float g_tailSlot[NTAIL], g_tailSlot30[NTAIL];  // RT_PTR: the float the instruction now reads, and its stock value

static int RateIndex(int rate) { return rate == 60 ? 0 : rate == 120 ? 1 : rate == 180 ? 2 : -1; }

// v3.1 COMPANIONS (research\framerate\v5\true60_companions.md section 2): the scale the companion hooks apply, set
// with the table: 30 / N while the table holds the N values, 1 at 30, at a rate not in the table and inside a 30 Hz
// gated update (TailWrite(-1)), so the hooks always agree with the table's per-tick constants.
static float g_cs = 1.0f;   // 30 / N: per-tick speeds, turn steps, float timers, haunting damage
static float g_ck2 = 1.0f;  // 2 (1 - 0.5^(30/N)): Cynthia's startled turn, whose step also halves every tick
static float g_cs2 = 1.0f;  // v3.2: (30 / N)^2: an acceleration per tick (the weapon ropes' sag)
static volatile int g_t60On;  // the table holds the N values
static void CompanionRate(int ri)
{
    const int rate = ri == 0 ? 60 : ri == 1 ? 120 : ri == 2 ? 180 : 30;
    g_t60On = ri >= 0;
    g_cs = 30.0f / (float)rate;
    g_cs2 = g_cs * g_cs;
    g_ck2 = 2.0f * (1.0f - powf(0.5f, g_cs));
}

// v2.1: the writes of TailApply without the log and the cache flush (x86 keeps code coherent with stores of the same
// process); the ENTITY TICK GATE calls it around every 30 Hz entity update (ri -1 = the stock 30 Hz values).
static void TailWrite(int ri)
{
    int i;
    if (!g_tailInstalled) return;
    for (i = 0; i < NTAIL; ++i) {
        const RateTail *e = &kTail[i];
        BYTE *p = (BYTE *)(ULONG_PTR)(e->va + e->off);
        if (!g_tailOk[i]) continue;
        if (e->type == RT_PTR) {
            g_tailSlot[i] = ri < 0 ? g_tailSlot30[i] : (float)e->v[ri];
        } else if (ri < 0) {
            memcpy(p, e->orig + e->off, e->type == RT_I8 ? 1 : e->type == RT_I16 ? 2 : 4);
        } else if (e->type == RT_F32) {
            const float f = (float)e->v[ri];
            memcpy(p, &f, 4);
        } else if (e->type == RT_I32) {
            const int n = (int)e->v[ri];
            memcpy(p, &n, 4);
        } else if (e->type == RT_I16) {
            const short n = (short)(int)e->v[ri];
            memcpy(p, &n, 2);
        } else {
            *p = (BYTE)(signed char)(int)e->v[ri];
        }
    }
    for (i = 0; i < NTAILRAW; ++i)
        if (g_tailRawOk[i]) memcpy((void *)(ULONG_PTR)kTailRaw[i].va, ri < 0 ? kTailRaw[i].orig : kTailRaw[i].per[ri], kTailRaw[i].len);
    CompanionRate(ri);  // v3.1
}

static void TailApply(int rate)
{
    const int ri = RateIndex(rate);
    int i;
    if (!g_tailInstalled) return;
    if (ri < 0 && rate != 30) {
        static int s_logged;
        if (!s_logged++) Log("frame rate: %d is not in the tables (60, 120, 180): the game's per-tick constants stay 30 Hz ones", rate);
    }
    for (i = 0; i < NTAIL; ++i) {
        const RateTail *e = &kTail[i];
        BYTE *p = (BYTE *)(ULONG_PTR)(e->va + e->off);
        if (!g_tailOk[i]) continue;
        if (e->type == RT_PTR) {
            g_tailSlot[i] = ri < 0 ? g_tailSlot30[i] : (float)e->v[ri];
        } else if (ri < 0) {
            memcpy(p, e->orig + e->off, e->type == RT_I8 ? 1 : e->type == RT_I16 ? 2 : 4);
        } else if (e->type == RT_F32) {
            const float f = (float)e->v[ri];
            memcpy(p, &f, 4);
        } else if (e->type == RT_I32) {
            const int n = (int)e->v[ri];
            memcpy(p, &n, 4);
        } else if (e->type == RT_I16) {
            const short n = (short)(int)e->v[ri];
            memcpy(p, &n, 2);
        } else {
            *p = (BYTE)(signed char)(int)e->v[ri];
        }
    }
    for (i = 0; i < NTAILRAW; ++i)
        if (g_tailRawOk[i]) memcpy((void *)(ULONG_PTR)kTailRaw[i].va, ri < 0 ? kTailRaw[i].orig : kTailRaw[i].per[ri], kTailRaw[i].len);
    CompanionRate(ri);  // v3.1
    FlushInstructionCache(GetCurrentProcess(), NULL, 0);
}

// B1 (fps_tail.md 3): fades set with `0x80 / fps` in integer maths get speed 0 at 180 and never end. At a rate other than
// 30 a speed of 0 or less becomes 1 (the quit-to-title fade at 0x415191 sets (0, 0) on purpose: kept).
static __declspec(naked) void hkFadeSet(void)  // 565CB0: void __cdecl (int target, int speed)
{
    __asm {
        mov eax, dword ptr [esp + 4]
        mov ecx, dword ptr [esp + 8]
        test ecx, ecx
        jg ok
        cmp dword ptr ds:[5D5248h], 30
        je ok
        cmp dword ptr [esp], 415196h
        je ok
        mov ecx, 1
    ok:
        mov dword ptr ds:[137F990h], eax
        mov dword ptr ds:[137F988h], ecx
        ret
    }
}

// B2: the spring helper 0x4216D0 (x*, v*, target, lo, hi, k, damp), per tick v = (v + rand(-lo..hi) - (x - t) k) damp,
// x += v; 37 callers, all floating / swaying monsters (Hummers, Victims, Tremer, Gum Head ...). At rate N with s = 30/N:
// k x s^2, lo / hi x s^1.5, damp^s. The caller's arguments are copied (never changed in place) and the body is run
// through a trampoline with the stolen bytes `mov eax,[esp+14h]; fld dword ptr [esp+10h]`.
static DWORD g_springArgs[7];
static const DWORD g_springBack = 0x4216D8;
static void __cdecl SpringScale(const DWORD *args)
{
    const int rate = *(volatile int *)0x5D5248;
    float lo, hi, k, damp, s;
    memcpy(g_springArgs, args, sizeof g_springArgs);
    if (rate == 30 || rate < 10 || rate > 1000) return;
    s = 30.0f / (float)rate;
    memcpy(&lo, &args[3], 4);
    memcpy(&hi, &args[4], 4);
    memcpy(&k, &args[5], 4);
    memcpy(&damp, &args[6], 4);
    lo *= s * sqrtf(s);
    hi *= s * sqrtf(s);
    k *= s * s;
    if (damp > 0.0f && damp < 1.0f) damp = powf(damp, s);
    memcpy(&g_springArgs[3], &lo, 4);
    memcpy(&g_springArgs[4], &hi, 4);
    memcpy(&g_springArgs[5], &k, 4);
    memcpy(&g_springArgs[6], &damp, 4);
}
static __declspec(naked) void SpringTramp(void)
{
    __asm {
        mov eax, dword ptr [esp + 14h]
        fld dword ptr [esp + 10h]
        jmp dword ptr [g_springBack]
    }
}
static __declspec(naked) void hkSpring(void)  // 4216D0, reached by jmp: [esp] = the caller's return address
{
    __asm {
        lea eax, [esp + 4]
        push eax
        call SpringScale
        add esp, 4
        push dword ptr [g_springArgs + 24]
        push dword ptr [g_springArgs + 20]
        push dword ptr [g_springArgs + 16]
        push dword ptr [g_springArgs + 12]
        push dword ptr [g_springArgs + 8]
        push dword ptr [g_springArgs + 4]
        push dword ptr [g_springArgs + 0]
        call SpringTramp
        add esp, 28
        ret
    }
}

// ================================================================ ENTITY TICK GATE (v2.1 prototype)
// research\framerate\v3\entity_speed.md. The monsters' and the companions' (Cynthia, Eileen) per-tick logic is only partly
// written for a variable rate (per-tick displacements in 28 monster functions, the companions' walk integrator 0x441AE0
// adds a speed per tick, per-tick chances / ramps / blends ...). Instead of finding every site, their scheduler tasks run
// at 30 Hz: the task loop 0x553850 calls [task+404] at 0x553877; for a gated update function the call happens only on
// ticks where a 30 Hz accumulator crosses, and then as a 30 Hz tick: [0x5D5248] = 30 (so 0x55AE50 / 0x55AE60, the
// animation step AnimStep and the spring hook see 30), the enemy dt block [0x614FEC..F4] for 30, and the per-tick tail
// at its stock values (TailWrite(-1)); everything is put back right after the call. Between those ticks the entity is
// left as it is (as the cheat table's Enemy Behaviour "Freeze" does, tested live). Henry, the camera, doors and events
// keep running every tick. EntitySmooth draws gated entities at positions interpolated between their last two 30 Hz
// states (the model's root position, set through the game's own setter 0x55C360 before the world is drawn and put back
// after). The haunting sum 0x539920 is taken on 30 Hz ticks only (cached x 30 / rate between them) so the auras that
// monsters register on their ticks do not flicker and do the stock damage.
enum { GK_NONE, GK_MONSTER, GK_COMPANION };
static const struct { DWORD fn; int kind; } kGateFn[] = {
    // monster update functions: slot 0 of the type descriptors 0x615348 + 16 x type (types 1..13h) ...
    { 0x476170, GK_MONSTER }, { 0x4AD030, GK_MONSTER }, { 0x470A80, GK_MONSTER }, { 0x4ABF80, GK_MONSTER },
    { 0x49D0C0, GK_MONSTER }, { 0x4CCA70, GK_MONSTER }, { 0x49BAE0, GK_MONSTER }, { 0x4C9950, GK_MONSTER },
    { 0x48BD30, GK_MONSTER }, { 0x492C00, GK_MONSTER }, { 0x4B2C30, GK_MONSTER }, { 0x4B8700, GK_MONSTER },
    { 0x482320, GK_MONSTER }, { 0x47DBA0, GK_MONSTER }, { 0x4C1260, GK_MONSTER }, { 0x4A4D60, GK_MONSTER },
    { 0x4C1340, GK_MONSTER }, { 0x4766B0, GK_MONSTER }, { 0x4771F0, GK_MONSTER },
    // ... and the extra tasks some types make: Hummer variant (0x4732B0), type 0Fh child (0x476BC0), Wall Man v2 parts
    // (0x4A0460), the Room 302 wall ghost (0x487840)
    { 0x470B20, GK_MONSTER }, { 0x476910, GK_MONSTER }, { 0x49FE10, GK_MONSTER }, { 0x49FDA0, GK_MONSTER },
    { 0x487650, GK_MONSTER },
    // companions: Cynthia (task [0xFD5A50], 0x4ED000; 0x4ED4F0 her scripted variant), Eileen (task [0xFCE6B4]: 0x44ADE0
    // normal, 0x44B110 final-boss walk, 0x44B3D0 idle in a room). All run the shared companion code 0x43B000..0x44E000.
    { 0x4ED000, GK_COMPANION }, { 0x4ED4F0, GK_COMPANION },
    { 0x44ADE0, GK_COMPANION }, { 0x44B110, GK_COMPANION }, { 0x44B3D0, GK_COMPANION },
};
#define NGATEFN ((int)(sizeof kGateFn / sizeof kGateFn[0]))

static int g_gateHooks;              // the task hook is in place
static volatile int g_gateTick = 1;  // this tick is a 30 Hz one (decided at the previous frame end)
static volatile int g_compTick = 1;  // v3.1: what a companion tick counter subtracts: g_gateTick with the N table, else 1
static int g_gateAcc;                // 30 Hz accumulator, 0 <= acc < rate: acc / rate = the part of a 30 Hz step gone by
static int g_gateRate = 30;          // the rate GateAdvance last saw
static DWORD g_gateTickNo;           // counts the 30 Hz ticks (smoothing: which entities stepped at the last one)
static int g_inGate;                 // inside a gated update
static float g_hauntLast, g_hauntK = 1.0f;  // haunting sum of the last 30 Hz tick, and 30 / rate

static int GateKind(DWORD fn)
{
    int i;
    for (i = 0; i < NGATEFN; ++i)
        if (kGateFn[i].fn == fn)  // v3.0: EntityGate = monsters only, CompanionGate = companions only
            return (kGateFn[i].kind == GK_COMPANION && !g_compGateIni) || (kGateFn[i].kind == GK_MONSTER && !g_gateIni)
                       ? GK_NONE : kGateFn[i].kind;
    return GK_NONE;
}

// Frame end (Governor, after any rate switch): decides whether the next tick is a 30 Hz one.
static void GateAdvance(int rate)
{
    if (rate != g_gateRate) {
        g_gateRate = rate;
        g_gateAcc = 0;
        g_hauntK = rate > 30 ? 30.0f / (float)rate : 1.0f;
    }
    if (rate <= 30 || rate > 1000) {
        g_gateTick = 1;
        g_gateAcc = 0;
    } else {
        g_gateAcc += 30;
        g_gateTick = g_gateAcc >= rate;
        if (g_gateTick) g_gateAcc -= rate;
    }
    if (g_gateTick) ++g_gateTickNo;
    g_compTick = g_t60On ? g_gateTick : 1;  // v3.1
}

// The engine as a 30 Hz tick (on = 1) or back at the rate in force (on = 0).
static int g_gateSavedRate;
static float g_gateSavedDt[3];
static void Gate30(int on)
{
    float *dt = (float *)0x614FEC;  // [614FEC] fps*k, [614FF0] 1/(fps*k), [614FF4] 1/(1+1/(fps*k)) (0x45E830, k = [614FE8])
    if (on) {
        int k = *(volatile int *)0x614FE8;
        float fk;
        if (k < 1 || k > 16) k = 1;
        fk = 30.0f * (float)k;
        g_gateSavedRate = *(volatile int *)0x5D5248;
        memcpy(g_gateSavedDt, dt, sizeof g_gateSavedDt);
        *(volatile int *)0x5D5248 = 30;
        dt[0] = fk;
        dt[1] = 1.0f / fk;
        dt[2] = 1.0f / (1.0f + 1.0f / fk);
        TailWrite(-1);
        g_inGate = 1;
    } else {
        *(volatile int *)0x5D5248 = g_gateSavedRate;
        memcpy(dt, g_gateSavedDt, sizeof g_gateSavedDt);
        TailWrite(RateIndex(g_gateSavedRate));
        g_inGate = 0;
    }
}

// ---- EntitySmooth: the root position of each gated entity at its last two 30 Hz steps
typedef struct { BYTE *task; DWORD fn; BYTE *model; DWORD tickNo; int applied; float prev[4], cur[4], shown[4]; } SmoothEnt;
#define NSMOOTH 64
static SmoothEnt g_sm[NSMOOTH];

static BYTE *GateModel(BYTE *task, int kind)
{
    BYTE *model = NULL;
    __try {
        BYTE *work = *(BYTE **)(task + 0x408);
        if (kind == GK_COMPANION) {
            if (work) model = *(BYTE **)(work + 0x18);  // the companion struct's model (0x43E450: [edi+18h])
        } else {
            const short type = *(short *)(task + 0x414);  // 0x45CBB0: types 1..12h keep it at work+30h, others task+40Ch
            model = type > 0 && type < 0x13 ? (work ? *(BYTE **)(work + 0x30) : NULL) : *(BYTE **)(task + 0x40C);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        model = NULL;
    }
    return (ULONG_PTR)model < 0x10000 ? NULL : model;
}

static void SmoothRecord(BYTE *task, DWORD fn, int kind)
{
    BYTE *model = GateModel(task, kind);
    SmoothEnt *e = NULL, *slot = NULL;
    float pos[4];
    int i;
    if (!model) return;
    __try {
        memcpy(pos, model + 0x10, sizeof pos);  // node position (0x419DE0 / 0x419E00)
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    for (i = 0; i < NSMOOTH; ++i) {
        if (g_sm[i].task == task) { e = &g_sm[i]; break; }
        if (!slot && (!g_sm[i].task || g_sm[i].tickNo + 2 < g_gateTickNo)) slot = &g_sm[i];
    }
    if (!e) {
        if (!slot) return;
        e = slot;
        e->task = NULL;
    }
    if (e->task != task || e->fn != fn || e->model != model || e->tickNo + 1 != g_gateTickNo) {
        memcpy(e->prev, pos, sizeof pos);  // new, or it missed a step: no in-between positions this time
    } else {
        const float dx = pos[0] - e->cur[0], dy = pos[1] - e->cur[1], dz = pos[2] - e->cur[2];
        if (dx * dx + dy * dy + dz * dz > 600.0f * 600.0f) memcpy(e->prev, pos, sizeof pos);  // a warp
        else memcpy(e->prev, e->cur, sizeof pos);
    }
    memcpy(e->cur, pos, sizeof pos);
    e->task = task;
    e->fn = fn;
    e->model = model;
    e->tickNo = g_gateTickNo;
    e->applied = 0;
}

typedef void (__cdecl *SetPosFn)(void *node, const float *pos);  // 0x55C360 -> 0x419E00 (marks the node dirty)
static void __cdecl SmoothApply(void)
{
    const int rate = *(volatile int *)0x5D5248;
    const float a = rate > 30 ? (float)g_gateAcc / (float)rate : 1.0f;
    int i;
    if (!g_smoothIni || !g_gateHooks || rate <= 30) return;
    for (i = 0; i < NSMOOTH; ++i) {
        SmoothEnt *e = &g_sm[i];
        if (!e->task || e->tickNo != g_gateTickNo) continue;
        __try {
            if (*(DWORD *)(e->task + 0x404) != e->fn || GateModel(e->task, GateKind(e->fn)) != e->model ||
                memcmp(e->model + 0x10, e->cur, sizeof e->cur))
                continue;  // gone, or moved since its step (a script): leave it
            e->shown[0] = e->prev[0] + (e->cur[0] - e->prev[0]) * a;
            e->shown[1] = e->prev[1] + (e->cur[1] - e->prev[1]) * a;
            e->shown[2] = e->prev[2] + (e->cur[2] - e->prev[2]) * a;
            e->shown[3] = e->cur[3];
            ((SetPosFn)0x55C360)(e->model, e->shown);
            e->applied = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            e->task = NULL;
        }
    }
}
static void __cdecl SmoothRestore(void)
{
    int i;
    for (i = 0; i < NSMOOTH; ++i) {
        SmoothEnt *e = &g_sm[i];
        if (!e->applied) continue;
        e->applied = 0;
        __try {
            if (!memcmp(e->model + 0x10, e->shown, sizeof e->shown)) ((SetPosFn)0x55C360)(e->model, e->cur);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            e->task = NULL;
        }
    }
}
// phase-0 list 0x62BC78: entry 7 (0x4EF670, the world's draw) and entry 14 (0x4EF940, 2D sprites, the last one)
static void __cdecl PhaseDrawBegin(void) { SmoothApply(); ((void (__cdecl *)(void))0x4EF670)(); }
static void __cdecl PhaseDrawEnd(void) { ((void (__cdecl *)(void))0x4EF940)(); SmoothRestore(); }

// ---- the task loop: 0x553877 `lea edx,[esi-404h]; push edx; call eax` (9 bytes) -> jmp hkTask
typedef void (__cdecl *TaskFn)(void *task);
static void __cdecl TaskRun(BYTE *task, TaskFn fn)
{
    const int kind = GateKind((DWORD)(ULONG_PTR)fn);
    const int rate = *(volatile int *)0x5D5248;
    if (kind == GK_NONE || rate <= 30 || rate > 1000 || g_inGate) {
        fn(task);
        return;
    }
    if (!g_gateTick) return;  // between two 30 Hz steps: the entity keeps its state
    Gate30(1);
    fn(task);
    Gate30(0);
    if (g_smoothIni) SmoothRecord(task, (DWORD)(ULONG_PTR)fn, kind);
}
static const DWORD g_taskBack = 0x553880;
static __declspec(naked) void hkTask(void)  // eax = [task+404], esi = task+404
{
    __asm {
        lea edx, [esi - 404h]
        push edx              // the game's argument: `add esp,4` at 553880 takes it off
        push eax
        push edx
        call TaskRun
        add esp, 8
        jmp dword ptr [g_taskBack]
    }
}

// ---- haunting sum: 0x5390CE `call 539920` (in 0x539080, Henry's aura / curse step) -> call hkHaunt
static const DWORD g_hauntFn = 0x539920;
static __declspec(naked) void hkHaunt(void)  // float __cdecl (int arg), result in st(0)
{
    __asm {
        cmp dword ptr ds:[5D5248h], 30
        jle direct
        cmp dword ptr [g_gateHooks], 0
        je direct
        cmp dword ptr [g_gateTick], 0
        je cached
        push dword ptr [esp + 4]
        call dword ptr [g_hauntFn]
        add esp, 4
        fst dword ptr [g_hauntLast]
        fmul dword ptr [g_hauntK]
        ret
    cached:
        fld dword ptr [g_hauntLast]
        fmul dword ptr [g_hauntK]
        ret
    direct:
        jmp dword ptr [g_hauntFn]
    }
}

static void GateInstall(void)
{
    static const Site kTask = { 0x553877, { 0x8D, 0x96, 0xFC, 0xFB, 0xFF, 0xFF, 0x52, 0xFF, 0xD0 }, 9 };
    static const Site kHaunt = { 0x5390CE, { 0xE8, 0x4D, 0x08, 0x00, 0x00 }, 5 };
    if (!g_gateIni && !g_compGateIni) {
        Log("entity gate: off (EntityGate=0, CompanionGate=0): monsters and companions step every tick (true rate)");
        return;
    }
    if (!SiteOk(&kTask)) {
        Log("entity gate: the task loop 00553877 is not stock (a cheat table script?): no gate");
        return;
    }
    PokeBranch(0x553877, 0xE9, hkTask, 9);
    g_gateHooks = 1;
    // v3.0 (research v5\true60_monsters.md): the haunting sum is rate-correct when monsters register every tick (each
    // registration adds strength x 1/fps); the 30 Hz cache is only right while monsters are gated
    if (g_gateIni) {
        if (SiteOk(&kHaunt)) PokeBranch(0x5390CE, 0xE8, hkHaunt, 5);
        else Log("entity gate: 005390CE is not stock: haunting auras of monsters are summed every tick");
    }
    if (g_smoothIni) {
        const DWORD b = (DWORD)(ULONG_PTR)PhaseDrawBegin, e = (DWORD)(ULONG_PTR)PhaseDrawEnd;
        if (*(DWORD *)0x62BC94 == 0x4EF670 && *(DWORD *)0x62BCB0 == 0x4EF940) {
            Poke(0x62BC94, &b, 4);
            Poke(0x62BCB0, &e, 4);
        } else {
            g_smoothIni = 0;
            Log("entity gate: the phase list 0062BC78 is not stock: no smoothing");
        }
    }
    Log("entity gate: %s step 30 times a second at any frame rate%s",
        g_gateIni && g_compGateIni ? "monsters and companions" : g_gateIni ? "monsters" : "companions",
        g_smoothIni ? ", drawn in between" : "");
}

// ================================================================ v3.0 TRUE 60 (research\framerate\v5\)
// FrameMode 0 with the entity gate off: monsters (and companions, see CompanionGate) run every tick at the full rate.
// The audits found monster movement and turning already scaled by the enemy dt except a short list (now rows of the
// per-tick table, area t60_*), plus the patches below. All of them equal the stock behaviour at 30.
#include "true60_fixed.h"
#define NT60CLIP ((int)(sizeof kT60Clip / sizeof kT60Clip[0]))
#define NT60BYTES ((int)(sizeof kT60Bytes / sizeof kT60Bytes[0]))

// (1) Henry's code turns a clip frame into seconds as frame x 0x55AE60() (1/fps). Clip frames are 1/30 s at any rate
// (AnimStep), so at N every hit / combo / item-use window opened N/30 times late: those calls get a constant 1/30.
static const float g_clipFrameSec = 1.0f / 30.0f;
static __declspec(naked) void ClipFrameSeconds(void)  // float __cdecl (void), like 0x55AE60 (touches no register)
{
    __asm {
        fld dword ptr [g_clipFrameSec]
        ret
    }
}

// (2) The ghost framework (0x483B60, every Victim / Andrew / Jasper / Richard) multiplies the hover velocity by its damp
// again after the spring call: 0x483DAC `fld [esi+20h]; fmul [edi+74h]` -> call hkGhostDamp; nop (st0 = damp^(30/N) x v,
// then the original fstp [edi+74h] at 0x483DB2). Inside a gated update the rate reads 30: the stock damp.
static float __cdecl DampPow(float d)
{
    static DWORD s_bits[8];
    static float s_val[8];
    static int s_rate[8], s_next;
    const int rate = *(volatile int *)0x5D5248;
    DWORD bits;
    int i;
    if (rate == 30 || rate < 10 || rate > 1000 || !(d > 0.0f && d < 1.0f)) return d;
    memcpy(&bits, &d, 4);
    for (i = 0; i < 8; ++i)
        if (s_bits[i] == bits && s_rate[i] == rate) return s_val[i];
    i = s_next++ & 7;
    s_bits[i] = bits;
    s_rate[i] = rate;
    s_val[i] = powf(d, 30.0f / (float)rate);
    return s_val[i];
}
static __declspec(naked) void hkGhostDamp(void)
{
    __asm {
        push ecx
        push edx
        push dword ptr [esi + 20h]
        call DampPow
        add esp, 4
        pop edx
        pop ecx
        fmul dword ptr [edi + 74h]
        ret
    }
}

// (3) Counters that advance one step per tick for a look, now only on the ticks where the 30 Hz accumulator crosses
// (g_gateTick, kept at every frame end by GateAdvance whether the gate is on or not; always 1 at 30):
// the health gauge pulse 0x50E3D0 (`mov eax,[esi+1D0h]; add esp,14h; inc eax; mov [esi+1D0h],eax`) and the placed Holy
// Candle's flame flipbook 0x53FCDA (`mov eax,[esi+0F4h]; add esp,18h; inc eax; push eax; mov [esi+0F4h],eax`).
static __declspec(naked) void hkGaugePulse(void)  // then `add esp,14h` (kept)
{
    __asm {
        mov eax, dword ptr [esi + 1D0h]
        add eax, dword ptr [g_gateTick]
        mov dword ptr [esi + 1D0h], eax
        ret
    }
}
static __declspec(naked) void hkCandleFlame(void)  // then `add esp,18h; push eax` (kept)
{
    __asm {
        mov eax, dword ptr [esi + 0F4h]
        add eax, dword ptr [g_gateTick]
        mov dword ptr [esi + 0F4h], eax
        ret
    }
}
// (3b) v3.2.3: the Holy Candle placement in the worlds (state 106 handler 0x540110, phase 2) runs its one-shot action
// on an EXACT clip frame: `call 417F50; cmp eax,edi; jne 5402BA` (0x540245 / 0x54024D), edi = 28 (kT60Bytes 0x54020C).
// The action places the candle, frees the candle object 0x10A40B0 (0x55C2D0) and zeroes the pointer. Clip time runs in
// 30 Hz frames, so at N fps frame 28 lasts N / 30 ticks: the second tick read the freed (NULL) object in 0x55C3B0
// (`mov eax,[eax+0E0h]`, the crash at 0x55C3B4 the user hit at 60+ fps when setting a candle down). It is the game's
// only exact-frame test on a player clip (0x417F50 + cmp/je|jne, whole exe). Now the action fires once per placement
// (latched until the clip leaves the frame) and never with a NULL object; at 30 it is the stock behaviour.
static BYTE g_candleFired;
static const DWORD g_candleGo = 0x540256, g_candleSkip = 0x5402BA;
static __declspec(naked) void hkCandlePlace(void)  // jmp from 0x54024D (cmp eax,edi; jne 5402BA; mov eax,[10A40B0])
{
    __asm {
        cmp eax, edi
        jne notframe
        cmp byte ptr [g_candleFired], 0
        jne skip
        mov byte ptr [g_candleFired], 1
        mov eax, dword ptr ds:[10A40B0h]
        test eax, eax
        jz skip
        jmp dword ptr [g_candleGo]   // push eax; call 55C3B0 ...
    notframe:
        mov byte ptr [g_candleFired], 0
    skip:
        jmp dword ptr [g_candleSkip]
    }
}
// (4) The Room 302 look icon's 8 echo copies take a new random scale every tick (0x4F4EF8 `call 421620`, loop index esi):
// a new one on 30 Hz ticks, the last one in between.
static float g_echoScale[16];
static const DWORD g_rand11 = 0x421620;
static __declspec(naked) void hkIconEcho(void)
{
    __asm {
        cmp esi, 16
        jae fresh
        cmp dword ptr [g_gateTick], 0
        je cached
        call dword ptr [g_rand11]
        fst dword ptr [g_echoScale + esi * 4]
        ret
    cached:
        fld dword ptr [g_echoScale + esi * 4]
        ret
    fresh:
        jmp dword ptr [g_rand11]
    }
}

// ================================================================ v3.1 COMPANIONS (research\framerate\v5\true60_companions.md)
// Cynthia's and Eileen's code (0x43B000..0x44E000, 0x4EB710..0x4ECEB0) never reads the clock: every number in it is per
// 30 Hz tick. Their per-tick constants are rows of the table (area t60_companion); these hooks fix the places where a
// per-tick amount is applied. Every number stays in 30 Hz units; g_cs (30 / N) and g_compTick (1 once per 1/30 s)
// are 1 at 30 and inside a 30 Hz gated update (CompanionGate=1), so the stock behaviour is kept there bit for bit.

// H1: the walk integrator 0x441AE0 is the only place a companion moves by its speed (units per 30 Hz tick, set by ~30
// state handlers): its three loads of the speed get x g_cs, so every walk / run / back-off state of both is fixed.
static const DWORD g_cwBack1 = 0x441B2A, g_cwBack2 = 0x441B5C, g_cwBack3 = 0x441637;
static __declspec(naked) void hkCompWalk1(void)  // 441B24: was `fld dword ptr [ebx+190h]`
{
    __asm {
        fld dword ptr [ebx + 190h]
        fmul dword ptr [g_cs]
        jmp dword ptr [g_cwBack1]
    }
}
static __declspec(naked) void hkCompWalk2(void)  // 441B56: was `fld dword ptr [ebx+198h]`
{
    __asm {
        fld dword ptr [ebx + 198h]
        fmul dword ptr [g_cs]
        jmp dword ptr [g_cwBack2]
    }
}
static __declspec(naked) void hkCompWalk3(void)  // 441631: was `fld dword ptr [ebx+190h]` (ground probe: speed / segment)
{
    __asm {
        fld dword ptr [ebx + 190h]
        fmul dword ptr [g_cs]
        jmp dword ptr [g_cwBack3]
    }
}

// H2: the per-tick heading step: 0x421B30(out, axis, angle) builds the rotation the turn code applies this tick. Six
// call sites pass a per-tick angle (Eileen 0x44719C / 0x44754C / 0x447B75, Cynthia 0x4EC2F3 / 0x4EC879, and her
// startled turn 0x4ECE57, whose step also halves every tick (table row 0x4ECE7B): x 2 (1 - 0.5^s) keeps its total).
static const DWORD g_rotYFn = 0x421B30;
static __declspec(naked) void hkRotYStep(void)
{
    __asm {
        fld dword ptr [esp + 0Ch]
        fmul dword ptr [g_cs]
        fstp dword ptr [esp + 0Ch]
        jmp dword ptr [g_rotYFn]
    }
}
static __declspec(naked) void hkRotYStepDecay(void)
{
    __asm {
        fld dword ptr [esp + 0Ch]
        fmul dword ptr [g_ck2]
        fstp dword ptr [esp + 0Ch]
        jmp dword ptr [g_rotYFn]
    }
}

// H4: a haunting near Eileen takes the aura intensity off her HP every tick (0x43FAB0 -> 0x43EB70 at 0x43FBF8). The
// argument x g_cs, then 0x43EB70 itself, so Playable Eileen's hook at its entry still sees every call.
static const DWORD g_eileenDmgFn = 0x43EB70;
static __declspec(naked) void hkCompDmg(void)
{
    __asm {
        fld dword ptr [esp + 4]
        fmul dword ptr [g_cs]
        fstp dword ptr [esp + 4]
        jmp dword ptr [g_eileenDmgFn]
    }
}

// H5 GateDec: integer tick timers with computed or random starts subtract g_compTick instead of 1, so they keep
// counting 30 Hz ticks (exact; values written by scripts, saves or Playable Eileen keep their meaning).
static const DWORD g_gdBack446900 = 0x446907, g_gdBack446D48 = 0x446D50, g_gdBack446E7B = 0x446E85,
                   g_gdBack447020 = 0x44702A, g_gdBack44957A = 0x449580, g_gdBack449624 = 0x44962A,
                   g_gdBack44CC9A = 0x44CCA4, g_gdBack44CD27 = 0x44CD31, g_gdBack44D085 = 0x44D08B,
                   g_gd44D140 = 0x44D140, g_gd44D14F = 0x44D14F, g_gd44D154 = 0x44D154, g_gd44D15C = 0x44D15C;
static __declspec(naked) void hkGd446900(void)  // was `dec word ptr [esi+298h]` (Eileen AI re-decide; a cmp follows)
{
    __asm {
        push eax
        mov eax, dword ptr [g_compTick]
        sub word ptr [esi + 298h], ax
        pop eax
        jmp dword ptr [g_gdBack446900]
    }
}
static __declspec(naked) void hkGd446D48(void)  // was `dec eax; mov word ptr [esi+21Ch],ax` (Eileen AI timer)
{
    __asm {
        sub eax, dword ptr [g_compTick]
        mov word ptr [esi + 21Ch], ax
        jmp dword ptr [g_gdBack446D48]
    }
}
static __declspec(naked) void hkGd446E7B(void)  // was `lea edx,[eax-1]; mov word ptr [esi+21Ah],dx`; jg uses `test ax,ax`
{
    __asm {
        mov edx, eax
        sub edx, dword ptr [g_compTick]
        mov word ptr [esi + 21Ah], dx
        test ax, ax
        jmp dword ptr [g_gdBack446E7B]
    }
}
static __declspec(naked) void hkGd447020(void)  // same at 447020
{
    __asm {
        mov edx, eax
        sub edx, dword ptr [g_compTick]
        mov word ptr [esi + 21Ah], dx
        test ax, ax
        jmp dword ptr [g_gdBack447020]
    }
}
static __declspec(naked) void hkGd44957A(void)  // was `dec dword ptr [esi+228h]`; jns follows (Eileen sob loop)
{
    __asm {
        push eax
        mov eax, dword ptr [g_compTick]
        sub dword ptr [esi + 228h], eax
        pop eax
        jmp dword ptr [g_gdBack44957A]
    }
}
static __declspec(naked) void hkGd449624(void)  // same at 449624
{
    __asm {
        push eax
        mov eax, dword ptr [g_compTick]
        sub dword ptr [esi + 228h], eax
        pop eax
        jmp dword ptr [g_gdBack449624]
    }
}
static __declspec(naked) void hkGd44CC9A(void)  // was `lea edx,[eax-1]; mov word ptr [edi+2F2h],dx`; jge uses `test ax,ax`
{
    __asm {
        mov edx, eax
        sub edx, dword ptr [g_compTick]
        mov word ptr [edi + 2F2h], dx
        test ax, ax
        jmp dword ptr [g_gdBack44CC9A]
    }
}
static __declspec(naked) void hkGd44CD27(void)  // was `lea ecx,[eax-1]; mov word ptr [edi+2F2h],cx`; jge uses `test ax,ax`
{
    __asm {
        mov ecx, eax
        sub ecx, dword ptr [g_compTick]
        mov word ptr [edi + 2F2h], cx
        test ax, ax
        jmp dword ptr [g_gdBack44CD27]
    }
}
static __declspec(naked) void hkGd44D085(void)  // was `dec eax; mov [0FCE7DCh],eax` (talk scheduler cooldown)
{
    __asm {
        sub eax, dword ptr [g_compTick]
        mov dword ptr ds:[0FCE7DCh], eax
        jmp dword ptr [g_gdBack44D085]
    }
}
static __declspec(naked) void hkGd44D133(void)  // was `mov eax,[esi-4]; test eax,eax` + `jle 44D140; dec eax; mov [esi-4],eax; jmp 44D15C`
{
    __asm {
        mov eax, dword ptr [esi - 4]
        test eax, eax
        jle cond
        sub eax, dword ptr [g_compTick]
        mov dword ptr [esi - 4], eax
        jmp dword ptr [g_gd44D15C]
    cond:
        jmp dword ptr [g_gd44D140]
    }
}
static __declspec(naked) void hkGd44D147(void)  // was `test eax,eax; je 44D14F; dec dword ptr [esi]; jmp 44D154`
{
    __asm {
        test eax, eax
        je reload
        push eax
        mov eax, dword ptr [g_compTick]
        sub dword ptr [esi], eax
        pop eax
        jmp dword ptr [g_gd44D154]
    reload:
        jmp dword ptr [g_gd44D14F]
    }
}

typedef struct { DWORD va; BYTE op; const void *to; int len; BYTE stock[10]; const char *what; } CompHook;
static void CompanionInstall(void)
{
    static const CompHook k[] = {
        { 0x441B24, 0xE9, hkCompWalk1, 6, { 0xD9, 0x83, 0x90, 0x01, 0x00, 0x00 }, "walk step" },
        { 0x441B56, 0xE9, hkCompWalk2, 6, { 0xD9, 0x83, 0x98, 0x01, 0x00, 0x00 }, "walk step (side)" },
        { 0x441631, 0xE9, hkCompWalk3, 6, { 0xD9, 0x83, 0x90, 0x01, 0x00, 0x00 }, "ground probe" },
        { 0x44719C, 0xE8, hkRotYStep, 5, { 0xE8, 0x8F, 0xA9, 0xFD, 0xFF }, "Eileen turn to target" },
        { 0x44754C, 0xE8, hkRotYStep, 5, { 0xE8, 0xDF, 0xA5, 0xFD, 0xFF }, "Eileen follow turn" },
        { 0x447B75, 0xE8, hkRotYStep, 5, { 0xE8, 0xB6, 0x9F, 0xFD, 0xFF }, "Eileen turn in place" },
        { 0x4EC2F3, 0xE8, hkRotYStep, 5, { 0xE8, 0x38, 0x58, 0xF3, 0xFF }, "Cynthia follow turn" },
        { 0x4EC879, 0xE8, hkRotYStep, 5, { 0xE8, 0xB2, 0x52, 0xF3, 0xFF }, "Cynthia turn in place" },
        { 0x4ECE57, 0xE8, hkRotYStepDecay, 5, { 0xE8, 0xD4, 0x4C, 0xF3, 0xFF }, "Cynthia startled turn" },
        { 0x43FBF8, 0xE8, hkCompDmg, 5, { 0xE8, 0x73, 0xEF, 0xFF, 0xFF }, "haunting damage to Eileen" },
        { 0x446900, 0xE9, hkGd446900, 7, { 0x66, 0xFF, 0x8E, 0x98, 0x02, 0x00, 0x00 }, "Eileen AI timer 298h" },
        { 0x446D48, 0xE9, hkGd446D48, 8, { 0x48, 0x66, 0x89, 0x86, 0x1C, 0x02, 0x00, 0x00 }, "Eileen AI timer 21Ch" },
        { 0x446E7B, 0xE9, hkGd446E7B, 10, { 0x8D, 0x50, 0xFF, 0x66, 0x89, 0x96, 0x1A, 0x02, 0x00, 0x00 }, "Eileen AI timer 21Ah" },
        { 0x447020, 0xE9, hkGd447020, 10, { 0x8D, 0x50, 0xFF, 0x66, 0x89, 0x96, 0x1A, 0x02, 0x00, 0x00 }, "Eileen AI timer 21Ah" },
        { 0x44957A, 0xE9, hkGd44957A, 6, { 0xFF, 0x8E, 0x28, 0x02, 0x00, 0x00 }, "Eileen sob loop" },
        { 0x449624, 0xE9, hkGd449624, 6, { 0xFF, 0x8E, 0x28, 0x02, 0x00, 0x00 }, "Eileen sob loop" },
        { 0x44CC9A, 0xE9, hkGd44CC9A, 10, { 0x8D, 0x50, 0xFF, 0x66, 0x89, 0x97, 0xF2, 0x02, 0x00, 0x00 }, "Eileen idle chatter" },
        { 0x44CD27, 0xE9, hkGd44CD27, 10, { 0x8D, 0x48, 0xFF, 0x66, 0x89, 0x8F, 0xF2, 0x02, 0x00, 0x00 }, "Eileen idle chatter" },
        { 0x44D085, 0xE9, hkGd44D085, 6, { 0x48, 0xA3, 0xDC, 0xE7, 0xFC, 0x00 }, "talk cooldown" },
        { 0x44D133, 0xE9, hkGd44D133, 5, { 0x8B, 0x46, 0xFC, 0x85, 0xC0 }, "talk entry delay" },
        { 0x44D147, 0xE9, hkGd44D147, 8, { 0x85, 0xC0, 0x74, 0x04, 0xFF, 0x0E, 0xEB, 0x05 }, "talk entry hold" },
    };
    int i, n = 0;
    // (the state-exit cooldown [354h], 0x44AF6A in the audit, is covered by the table's start values 0x4470FD / 0x447201)
    for (i = 0; i < (int)(sizeof k / sizeof k[0]); ++i)
        if (!memcmp((const void *)(ULONG_PTR)k[i].va, k[i].stock, k[i].len) && PokeBranch(k[i].va, k[i].op, k[i].to, k[i].len))
            ++n;
        else
            Log("companions: %08lX (%s) is not stock: left", k[i].va, k[i].what);
    Log("companions: %d/%d hooks (walk, turn, haunting damage, tick timers)%s", n, (int)(sizeof k / sizeof k[0]),
        g_compGateIni ? "; CompanionGate=1: they still think 30 times a second" : "");
}

// ================================================================ v3.2 WEAPON ROPES + CUTSCENE STREAM
// (1) Eileen's weapon object (task 0x4456C0, the one Playable Eileen spawns for her and the game for the companion)
// hangs a rope from her hand for two weapons: type 1, the purse (item 0x80; 0x4443B0, 3 nodes, damp 0.95, sag 5) and
// type 4, the chain (0x84; 0x4448B0, 8 nodes, damp 0.9, sag 4), set up by 0x443DF0. 0x443EB0 steps one node per tick as Verlet:
//   x' = x + d (x - x_prev) + g (0, 1, 0)      d = [ctx+2A4h], g = [node+34h] (the sag x 1.1 per node), +y is down
// then holds it at its length from the node above. Both numbers are per 30 Hz tick and the fps table had no row for
// them: at N the rope fell (N/30)^2 times as hard (36x at 180) and lost its swing N/30 times as fast, so it hung
// straight down whatever she did. d^(30/N) and g x (30/N)^2 keep its motion per second; both follow the table's scale
// (1 at 30 and inside a gated update). The riding crop's wobble (type 3, 0x83: 0x444A00) is in the table.
static const DWORD g_ropeBackD = 0x443EFF, g_ropeBackG = 0x443F43;
static float __cdecl RopeDamp(float d)
{
    static float s_cs = 1.0f, s_in[4], s_out[4];
    static int s_next;
    int i;
    if (g_cs == 1.0f || !(d > 0.0f && d < 1.0f)) return d;
    if (s_cs != g_cs) {
        s_cs = g_cs;
        memset(s_in, 0, sizeof s_in);
    }
    for (i = 0; i < 4; ++i)
        if (s_in[i] == d) return s_out[i];
    i = s_next++ & 3;
    s_in[i] = d;
    s_out[i] = powf(d, g_cs);
    return s_out[i];
}
static __declspec(naked) void hkRopeDamp(void)  // 443EF9: was `fld dword ptr [ecx+2A4h]` (the FPU stack is empty here)
{
    __asm {
        push ecx
        push edx
        push dword ptr [ecx + 2A4h]
        call RopeDamp
        add esp, 4
        pop edx
        pop ecx
        jmp dword ptr [g_ropeBackD]
    }
}
static __declspec(naked) void hkRopeSag(void)  // 443F3D: was `fld dword ptr [esi+1Ch]; fld dword ptr [esi+34h]`
{
    __asm {
        fld dword ptr [esi + 1Ch]
        fld dword ptr [esi + 34h]
        fmul dword ptr [g_cs2]
        jmp dword ptr [g_ropeBackG]
    }
}

// (1b) v3.2.1 (main rig, 2026-11-06): the ropes step at 30 Hz. The user at 60-180: "the smallest idle movements and
// even breathing make them teeter and spin". d^s and g x s^2 above are right for those two terms, but the stock rope is
// calm mainly because of a third, hidden damping: after the Verlet step 0x443EB0 puts each node back at its length
// from the node above (x = parent + L normalize(pred - parent)), and that projection takes energy out in proportion to
// the angle swept per tick (0.45-0.62 rad per tick at 30). It shrinks with dt, so no constant can carry it over.
// Measured at 180 with her standing still: chain tip 40 deg off vertical on average (up to 90), 1225 u/s relative to the
// hand; the solver as disassembled, fed the recorded hand path, gives 39.5 deg / 1191 at 180, 18.5 / 496 at 60 and
// 11.1 / 247 at 30 (research: Playable Eileen Notes\session8\framerate_bugs.md, KB section 66).
// So, as the entity gate does for monsters: the stock solver 0x443FF0 runs only on 30 Hz gate ticks with the stock
// constants (hkRopeDamp / hkRopeSag are not installed), from this mod's copy of the rope state, and every tick in
// between shows the nodes part of the way from the last step to this one (the gate accumulator, as SmoothApply), moved
// with the hand so node 0 stays on it. The shape lags one 30 Hz step (33 ms); simulated, the chain at 60 / 120 / 180 is
// 11.04-11.08 deg / 247 u/s, as at 30. Hooks: the `call 0x443FF0` of the purse (0x4445A3, type 1) and the chain
// (0x4449DF, type 4); eax = the hand point, esi = the rope (nodes of 40h bytes from +0: pos +0..+0Ch, previous pos
// +10h..+1Ch, Verlet scratch +20h..+2Ch, length +30h, sag +34h; node count +2A0h, damping +2A4h). What the game changes
// in the nodes between two calls is carried into the rope state: the purse's wall push (0x444469.., before the call,
// moves pos and previous pos together: added up) and its node-2 direction clamp in 0x444030 (after the call, pos only:
// the latest one counts, it is a projection). At 30, above 1000, inside a gated update or with [Window] RopeStep30=0
// (hidden; then v3.2's two hooks are used) the call is the stock one.
#define ROPE_MAXN 16
typedef struct {
    BYTE *ctx;
    int n, rate;
    DWORD tick;                                     // [13DC324] of the last call
    float pos[ROPE_MAXN][4], prev[ROPE_MAXN][4];    // the rope's state after its last 30 Hz step
    float from[ROPE_MAXN][4];                       // the positions before that step (where the in-between pictures start)
    float wpos[ROPE_MAXN][4], wprev[ROPE_MAXN][4];  // what this mod wrote into the nodes at the last call
    float push[ROPE_MAXN][3], clamp[ROPE_MAXN][3];  // carried changes (see above)
} RopeState;
static RopeState g_rope[4];
static int g_ropeStep30 = 1;  // [Window] RopeStep30 (hidden)
static int g_ropeNext;

static __declspec(naked) void __cdecl RopeStock(const float *anchor, BYTE *ctx)  // 0x443FF0: eax = anchor, esi = rope
{
    __asm {
        push esi
        mov eax, dword ptr [esp + 8]
        mov esi, dword ptr [esp + 12]
        mov ecx, 443FF0h
        call ecx
        pop esi
        ret
    }
}

static void RopeLoad(RopeState *r, BYTE *ctx, int n, int rate, DWORD tick)
{
    int i;
    r->ctx = ctx;
    r->n = n;
    r->rate = rate;
    r->tick = tick;
    for (i = 0; i < n; ++i) {
        const float *node = (const float *)(ctx + 0x40 * i);
        memcpy(r->pos[i], node, 16);
        memcpy(r->prev[i], node + 4, 16);
        memcpy(r->from[i], node, 16);
        memcpy(r->wpos[i], node, 16);
        memcpy(r->wprev[i], node + 4, 16);
    }
    memset(r->push, 0, sizeof r->push);
    memset(r->clamp, 0, sizeof r->clamp);
}

// The rope state still belongs to this rope: same rope, same node count, and the previous positions are the ones this
// mod wrote (a rope set up again by 0x443DF0, a new weapon object at the same address, writes its own). A pause or a
// menu in between does not matter: the rope's time stood still too.
static int RopeOurs(const RopeState *r, BYTE *ctx, int n)
{
    int i;
    if (r->ctx != ctx || r->n != n) return 0;
    for (i = 1; i < n; ++i) {
        const float *node = (const float *)(ctx + 0x40 * i);
        const float *q = r->wprev[i];
        const float dx = node[4] - q[0], dy = node[5] - q[1], dz = node[6] - q[2];
        if (dx * dx + dy * dy + dz * dz > 50.0f * 50.0f) return 0;  // a wall push is a few units; a new rope is not
    }
    return 1;
}

// What the game did to the nodes since the last call, carried into the rope state.
static void RopeCarry(RopeState *r, BYTE *ctx)
{
    int i, k;
    for (i = 1; i < r->n; ++i) {
        const float *node = (const float *)(ctx + 0x40 * i);
        for (k = 0; k < 3; ++k) {
            const float dp = node[k] - r->wpos[i][k], dq = node[4 + k] - r->wprev[i][k];
            r->push[i][k] += dq;       // pos and previous pos moved together: a push
            r->clamp[i][k] = dp - dq;  // pos alone: the latest projection
        }
    }
}

// Puts the rope state (with the carried changes) into the nodes, as the stock solver expects it.
static void RopeRestore(RopeState *r, BYTE *ctx)
{
    int i, k;
    for (i = 0; i < r->n; ++i) {
        float *node = (float *)(ctx + 0x40 * i);
        for (k = 0; k < 3; ++k) {
            r->pos[i][k] += r->push[i][k] + r->clamp[i][k];
            r->prev[i][k] += r->push[i][k];
        }
        memcpy(node, r->pos[i], 16);
        memcpy(node + 4, r->prev[i], 16);
    }
    memset(r->push, 0, sizeof r->push);
    memset(r->clamp, 0, sizeof r->clamp);
}

static void __cdecl RopeCall(const float *anchor, BYTE *ctx)
{
    const int rate = *(volatile int *)0x5D5248;
    const DWORD tick = *(volatile DWORD *)0x13DC324;
    RopeState *r = NULL;
    float a, base[3];
    int i, k, n = *(int *)(ctx + 0x2A0);
    for (i = 0; i < 4; ++i)
        if (g_rope[i].ctx == ctx) { r = &g_rope[i]; break; }
    if (!g_ropeStep30 || rate <= 30 || rate > 1000 || g_inGate || n < 1 || n > ROPE_MAXN) {
        if (r) {
            if (RopeOurs(r, ctx, n)) {  // back to the stock step: hand it the real rope, not the last picture
                RopeCarry(r, ctx);
                RopeRestore(r, ctx);
            }
            r->ctx = NULL;
        }
        RopeStock(anchor, ctx);
        return;
    }
    if (!r) {
        for (i = 0; i < 4 && !r; ++i)
            if (!g_rope[i].ctx || g_rope[i].tick + 600 < tick) r = &g_rope[i];
        if (!r) r = &g_rope[g_ropeNext++ & 3];
        r->ctx = NULL;
    }
    if (!RopeOurs(r, ctx, n))
        RopeLoad(r, ctx, n, rate, tick);  // new: from the game's state, no in-between pictures until its next step
    else
        RopeCarry(r, ctx);
    r->tick = tick;
    r->rate = rate;
    if (g_gateTick) {  // a 30 Hz step from the rope's own state, with the stock constants
        RopeRestore(r, ctx);
        for (i = 0; i < n; ++i)
            memcpy(r->from[i], r->pos[i], 16);
        RopeStock(anchor, ctx);
        for (i = 0; i < n; ++i) {
            const float *node = (const float *)(ctx + 0x40 * i);
            memcpy(r->pos[i], node, 16);
            memcpy(r->prev[i], node + 4, 16);
        }
    }
    // the picture of this tick: part a of the way from the last step's start to its end, node 0 on the hand
    a = (float)g_gateAcc / (float)rate;
    for (k = 0; k < 3; ++k)
        base[k] = anchor[k] - (r->from[0][k] + (r->pos[0][k] - r->from[0][k]) * a);
    for (i = 0; i < n; ++i) {
        float *node = (float *)(ctx + 0x40 * i);
        if (i == 0) {
            memcpy(node, anchor, 16);  // as 0x443FF0 does
        } else {
            for (k = 0; k < 3; ++k)
                node[k] = r->from[i][k] + (r->pos[i][k] - r->from[i][k]) * a + base[k];
            node[3] = r->pos[i][3];
        }
        memcpy(node + 4, r->prev[i], 16);
        memcpy(r->wpos[i], node, 16);
        memcpy(r->wprev[i], node + 4, 16);
    }
}
static __declspec(naked) void hkRopeCall(void)  // 0x4445A3 / 0x4449DF: was `call 443FF0h` (eax = hand point, esi = rope)
{
    __asm {
        push esi
        push eax
        call RopeCall
        add esp, 8
        ret
    }
}

// (2) Real-time cutscenes play their stream (characters, props) by a clock that 0x560A6E advances by [0x12E7C50] per
// tick. Its only writer, 0x5600B0, sets it to 30 / fps once, when 0x4F1D00 loads the scene: one tick BEFORE the scene
// enters phase 1, where this mod runs it at 30. Loaded at N and played at 30, the characters moved at 30/N speed (1/6
// at 180) while the camera kept its own schedule and then swung to the next character on time. The step is now
// 30 / the rate in force each tick, the stock value at any steady rate, also for the in-world scenes that play at the
// gameplay rate and across a rate change while a scene plays.
static const DWORD g_streamBack = 0x560A74;
static const float g_thirty = 30.0f;
static float g_streamStep;
static __declspec(naked) void hkStreamStep(void)  // 560A6E: was `fld dword ptr ds:[12E7C50h]`; `test eax,eax` follows
{
    __asm {
        cmp dword ptr ds:[5D5248h], 10
        jl stock
        cmp dword ptr ds:[5D5248h], 1000
        jg stock
        fild dword ptr ds:[5D5248h]
        fdivr dword ptr [g_thirty]
        fstp dword ptr [g_streamStep]  // rounded to a float like the stock variable
        fld dword ptr [g_streamStep]
        jmp dword ptr [g_streamBack]
    stock:
        fld dword ptr ds:[12E7C50h]
        jmp dword ptr [g_streamBack]
    }
}

static void RopeStreamInstall(void)
{
    // v3.2.1: RopeStep30=1 (default) wraps the two solver calls; RopeStep30=0 is v3.2 (damping and sag per tick)
    static const CompHook kStep[] = {
        { 0x4445A3, 0xE8, hkRopeCall, 5, { 0xE8, 0x48, 0xFA, 0xFF, 0xFF }, "purse rope step (30 Hz)" },
        { 0x4449DF, 0xE8, hkRopeCall, 5, { 0xE8, 0x0C, 0xF6, 0xFF, 0xFF }, "chain rope step (30 Hz)" },
        { 0x560A6E, 0xE9, hkStreamStep, 6, { 0xD9, 0x05, 0x50, 0x7C, 0x2E, 0x01 }, "cutscene stream step" },
    };
    static const CompHook kPerTick[] = {
        { 0x443EF9, 0xE9, hkRopeDamp, 6, { 0xD9, 0x81, 0xA4, 0x02, 0x00, 0x00 }, "weapon rope damping" },
        { 0x443F3D, 0xE9, hkRopeSag, 6, { 0xD9, 0x46, 0x1C, 0xD9, 0x46, 0x34 }, "weapon rope sag" },
        { 0x560A6E, 0xE9, hkStreamStep, 6, { 0xD9, 0x05, 0x50, 0x7C, 0x2E, 0x01 }, "cutscene stream step" },
    };
    const CompHook *k;
    int i, n = 0, cnt;
    g_ropeStep30 = (int)GetPrivateProfileIntA("Window", "RopeStep30", 1, g_iniPath) != 0;
    k = g_ropeStep30 ? kStep : kPerTick;
    cnt = g_ropeStep30 ? (int)(sizeof kStep / sizeof kStep[0]) : (int)(sizeof kPerTick / sizeof kPerTick[0]);
    for (i = 0; i < cnt; ++i)
        if (!memcmp((const void *)(ULONG_PTR)k[i].va, k[i].stock, k[i].len) && PokeBranch(k[i].va, k[i].op, k[i].to, k[i].len))
            ++n;
        else
            Log("frame rate: %08lX (%s) is not stock: left", k[i].va, k[i].what);
    Log("frame rate: %d/%d hooks for Eileen's weapon ropes (%s) and the cutscene stream clock", n, cnt,
        g_ropeStep30 ? "a 30 Hz step drawn in between" : "RopeStep30=0: v3.2's per-tick damping and sag");
}

static void True60Install(void)
{
    static const Site kDamp = { 0x483DAC, { 0xD9, 0x46, 0x20, 0xD8, 0x4F, 0x74 }, 6 };
    static const Site kGauge = { 0x50E3D0, { 0x8B, 0x86, 0xD0, 0x01, 0x00, 0x00, 0x83, 0xC4, 0x14, 0x40, 0x89, 0x86, 0xD0,
                                             0x01, 0x00, 0x00 }, 16 };
    static const Site kCandle = { 0x53FCDA, { 0x8B, 0x86, 0xF4, 0x00, 0x00, 0x00, 0x83, 0xC4, 0x18, 0x40, 0x50, 0x89, 0x86,
                                              0xF4, 0x00, 0x00, 0x00 }, 17 };
    static const Site kEcho = { 0x4F4EF8, { 0xE8, 0x23, 0xC7, 0xF2, 0xFF }, 5 };
    static const Site kCandlePlace = { 0x54024D, { 0x3B, 0xC7, 0x75, 0x69, 0xA1, 0xB0, 0x40, 0x0A, 0x01 }, 9 };
    int i, clip = 0, bytes = 0, hooks = 0;
    for (i = 0; i < NT60CLIP; ++i)
        if (!memcmp((const void *)(ULONG_PTR)kT60Clip[i].va, kT60Clip[i].orig, 5) &&
            PokeBranch(kT60Clip[i].va, 0xE8, ClipFrameSeconds, 5))
            ++clip;
        else
            Log("true 60: %08lX is not stock (clip seconds left at 1/fps)", kT60Clip[i].va);
    for (i = 0; i < NT60BYTES; ++i)
        if (!memcmp((const void *)(ULONG_PTR)kT60Bytes[i].va, kT60Bytes[i].orig, kT60Bytes[i].len) &&
            Poke(kT60Bytes[i].va, kT60Bytes[i].now, kT60Bytes[i].len))
            ++bytes;
        else
            Log("true 60: %08lX (%s) is not stock: left", kT60Bytes[i].va, kT60Bytes[i].what);
    if (SiteOk(&kDamp) && PokeBranch(0x483DAC, 0xE8, hkGhostDamp, 6)) ++hooks;
    else Log("true 60: 00483DAC (ghost hover damping) is not stock: left");
    if (SiteOk(&kGauge)) {
        static const BYTE tail[] = { 0x83, 0xC4, 0x14, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };  // at 0x50E3D6
        if (PokeBranch(0x50E3D0, 0xE8, hkGaugePulse, 6) && Poke(0x50E3D6, tail, sizeof tail)) ++hooks;
    } else Log("true 60: 0050E3D0 (health gauge pulse) is not stock: left");
    if (SiteOk(&kCandle)) {
        static const BYTE tail[] = { 0x83, 0xC4, 0x18, 0x50, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };  // at 0x53FCDF
        if (PokeBranch(0x53FCDA, 0xE8, hkCandleFlame, 5) && Poke(0x53FCDF, tail, sizeof tail)) ++hooks;
    } else Log("true 60: 0053FCDA (Holy Candle flame) is not stock: left");
    if (SiteOk(&kEcho) && PokeBranch(0x4F4EF8, 0xE8, hkIconEcho, 5)) ++hooks;
    else Log("true 60: 004F4EF8 (look icon echoes) is not stock: left");
    if (SiteOk(&kCandlePlace)) {  // v3.2.3
        static const BYTE nops[] = { 0x90, 0x90, 0x90, 0x90 };  // 0x540252-0x540255: the rest of `mov eax,[10A40B0]`
        if (PokeBranch(0x54024D, 0xE9, hkCandlePlace, 5) && Poke(0x540252, nops, sizeof nops)) ++hooks;
    } else Log("true 60: 0054024D (Holy Candle placement frame) is not stock: left");
    Log("true 60: %d/%d clip-time calls at 1/30, %d/%d clip-frame tests, %d/5 hooks", clip, NT60CLIP, bytes, NT60BYTES, hooks);
}

// Installed at the first frame end, after every dll's DllMain (Playable Eileen checks its own sites first), each site
// checked against the GOG v7 bytes; a pair of sites (two halves of one smoother) goes in only together.
static void TailInstall(void)
{
    static const Site kB1 = { 0x565CB0, { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x4C, 0x24, 0x08, 0xA3, 0x90, 0xF9, 0x37, 0x01,
                                          0x89, 0x0D, 0x88, 0xF9, 0x37, 0x01, 0xC3 }, 20 };
    static const Site kB2 = { 0x4216D0, { 0x8B, 0x44, 0x24, 0x14, 0xD9, 0x44, 0x24, 0x10 }, 8 };
    int i, j, ok = 0, bad = 0;
    DWORD old;
    for (i = 0; i < NTAIL; ++i)
        g_tailOk[i] = !memcmp((const void *)(ULONG_PTR)kTail[i].va, kTail[i].orig, kTail[i].len);
    for (i = 0; i < NTAIL; ++i)  // pairs: both or neither
        if (!g_tailOk[i] && kTail[i].group)
            for (j = 0; j < NTAIL; ++j)
                if (kTail[j].group == kTail[i].group) g_tailOk[j] = 0;
    for (i = 0; i < NTAILRAW; ++i)
        g_tailRawOk[i] = !memcmp((const void *)(ULONG_PTR)kTailRaw[i].va, kTailRaw[i].orig, kTailRaw[i].len);
    if (!g_tailRawOk[0] || !g_tailRawOk[NTAILRAW - 1])  // the two halves of one division
        for (i = 0; i < NTAILRAW; ++i) g_tailRawOk[i] = 0;
    for (i = 0; i < NTAIL; ++i) {
        const RateTail *e = &kTail[i];
        if (!g_tailOk[i]) {
            ++bad;
            Log("frame rate: %08lX is not stock; left at its 30 Hz value", e->va);
            continue;
        }
        if (e->type == RT_PTR) {
            const DWORD to = (DWORD)(ULONG_PTR)&g_tailSlot[i];
            const float *from = *(const float **)(ULONG_PTR)(e->va + e->off);
            g_tailSlot30[i] = g_tailSlot[i] = *from;
            if (!Poke(e->va + e->off, &to, 4)) {
                g_tailOk[i] = 0;
                ++bad;
                continue;
            }
        } else {  // written again at every switch: the page stays writable
            if (!VirtualProtect((void *)(ULONG_PTR)e->va, e->len, PAGE_EXECUTE_READWRITE, &old)) {
                g_tailOk[i] = 0;  // review L1: never written then
                ++bad;
                continue;
            }
            if (e->va < 0x5B7000) ListPatch(e->va, e->len);
        }
        ++ok;
    }
    for (i = 0; i < NTAILRAW; ++i)
        if (g_tailRawOk[i] && !VirtualProtect((void *)(ULONG_PTR)kTailRaw[i].va, kTailRaw[i].len, PAGE_EXECUTE_READWRITE, &old))
            g_tailRawOk[0] = g_tailRawOk[NTAILRAW - 1] = 0;  // the pair goes together
    for (i = 0; i < NTAILRAW; ++i)
        if (g_tailRawOk[0] && g_tailRawOk[NTAILRAW - 1]) ListPatch(kTailRaw[i].va, kTailRaw[i].len);
    if (!g_tailRawOk[0] || !g_tailRawOk[NTAILRAW - 1])
        for (i = 0; i < NTAILRAW; ++i) g_tailRawOk[i] = 0;
    if (SiteOk(&kB1)) PokeBranch(0x565CB0, 0xE9, hkFadeSet, 20);
    else Log("frame rate: the fade setter 00565CB0 is not stock: fades set with 0x80 / fps may not end at 180");
    if (SiteOk(&kB2)) PokeBranch(0x4216D0, 0xE9, hkSpring, 8);
    else Log("frame rate: the spring helper 004216D0 is not stock: floating monsters keep their 30 Hz springs");
    g_tailInstalled = 1;
    GateInstall();  // v2.1 ENTITY TICK GATE
    True60Install();  // v3.0
    CompanionInstall();  // v3.1
    RopeStreamInstall();  // v3.2
    Log("frame rate: %d per-tick constants of the game follow the rate (%d not stock, left), %d raw", ok, bad,
        g_tailRawOk[0] ? NTAILRAW : 0);
}

static void FpsUserChanged(int n)
{
    const int eff = g_fpsCap && n > g_fpsCap ? g_fpsCap : n;  // the rate gameplay will really run at
    if (g_frameMode) {  // v2.1: the display rate; the logic (and Playable Eileen's GameRate) stays 30
        Interp_SetRate(eff);  // v3.12.0 (review GFX-001): the V-Sync cap holds here too
        Log("frame rate: %d chosen, %d shown (display, by interpolation; the game runs at 30)%s", n, eff, g_rateHooks ? "" : " -- not available");
        return;
    }
    InterlockedExchange(&g_fpsUser, n);
    if (g_rateHooks) RateWrite(0x40316F, &eff, 4);  // Playable Eileen's GameRate() (the instruction ran once at boot)
    Log("frame rate: %d chosen; %s", n, g_rateHooks ? "it applies when gameplay resumes" : "it applies at the next start");
}

// With V-Sync in force a rate above the refresh runs the game in slow motion (one logic tick per Present). Until
// v3.11.0 only exclusive full screen (Enable=0) was capped; v3.12.0: whenever V-Sync is effectively on (the V-Sync row,
// or Default with dxcfg.ini vsync=enabled; "application" = the game's own choice: VSync in full screen only), at the
// refresh of the monitor the game is on.
static int VsyncEffective(void)  // v3.39 PR339: dxcfg.ini alone (the V-Sync row is gone)
{
    char path[MAX_PATH], v[32];
    sprintf_s(path, sizeof path, "%s\\dxcfg.ini", g_dir);
    GetPrivateProfileStringA("general", "vsync", "application", v, sizeof v, path);
    if (!_stricmp(v, "enabled")) return 1;
    if (!_stricmp(v, "disabled")) return 0;
    return !g_enable;
}
static int FpsCapFor(unsigned hz)  // the highest table rate at or below hz (59.94 Hz modes report 59: 2 Hz slack); 0 = none
{
    int i, cap = 30;
    if (hz < 30) return 0;
    for (i = 0; i < FPS_N; ++i)
        if (kFps[i] <= (int)hz + 2) cap = kFps[i];
    return cap;
}
static void FpsCapUpdate(void)  // the one cap for both frame modes (review GFX-003: interp.c no longer caps by itself)
{
    const int vs = VsyncEffective(), hz = MonitorRefresh();  // the monitor the game is on
    const int cap = vs ? FpsCapFor((unsigned)hz) : 0;
    g_fpsCap = cap >= kFps[FPS_N - 1] ? 0 : cap;
    Log("frame rate: V-Sync %s (dxcfg.ini), monitor %d Hz: %s %d", vs ? "on" : "off",  // v3.39 PR339: always dxcfg.ini's
        hz, g_fpsCap ? "rates capped at" : "no cap, up to", g_fpsCap ? g_fpsCap : kFps[FPS_N - 1]);
}
static void FpsCapRefresh(const char *why)  // V-Sync applied / monitor changed: the rate in force follows at once
{
    FpsCapUpdate();
    if (g_fpsRow) FpsUserChanged(kFps[g_fpsSel]);
    (void)why;
}

static void ApplyFrameRate(void)  // DllMain, before the exe's init
{
    static const Site kSites[] = {
        { 0x403169, { 0xC7, 0x05, 0x48, 0x52, 0x5D, 0x00, 0x1E, 0x00, 0x00, 0x00 }, 10 },
        { 0x414F0E, { 0xA3, 0xDC, 0x0D, 0x61, 0x00 }, 5 },
        { 0x415202, { 0xE8, 0xE9, 0x59, 0x14, 0x00 }, 5 },
        { 0x4184DC, { 0x8B, 0x46, 0x38, 0x74, 0x27 }, 5 },
        { 0x4192B5, { 0x8B, 0x46, 0x38, 0x74, 0x27 }, 5 },
        { 0x41848E, { 0x8B, 0x44, 0x24, 0x0C, 0x8B, 0x56, 0x34 }, 7 },
    };
    int n = kFps[FpsIndexOf(g_fpsIni)];
    int i, bad = 0;
    for (i = 0; i < (int)(sizeof kSites / sizeof kSites[0]); ++i)
        if (!SiteOk(&kSites[i])) ++bad;
    g_fpsRow = !bad;
    g_fpsSel = FpsIndexOf(g_fpsIni);
    g_fpsUser = n;
    if (bad) {
        Log("frame rate: %d site(s) differ from the GOG v7 exe (another frame-rate mod?): no Frame Rate row, the game keeps its rate", bad);
        return;
    }
    for (i = 0; i < (int)(sizeof kRateImm8 / sizeof kRateImm8[0]); ++i) {
        g_rateImmOk[i] = !memcmp((const void *)(ULONG_PTR)kRateImm8[i].site, kRateImm8[i].stock, 3);
        if (g_rateImmOk[i]) ListPatch(kRateImm8[i].site, 3);
        else Log("frame rate: %08lX (%s) is not stock; left at its 30 Hz value", kRateImm8[i].site, kRateImm8[i].what);
    }
    FpsCapUpdate();  // v3.12.0: before both branches (review GFX-001)
    if (g_frameMode) {  // v2.1: the row picks the display rate; the logic stays at 30 (0x40316F keeps the stock 30)
        InterpStart(g_fpsCap && n > g_fpsCap ? g_fpsCap : n);
        g_fpsUser = n = 30;
    } else {
        if (g_fpsCap && n > g_fpsCap) n = g_fpsCap;
        Poke(0x40316F, &n, 4);  // the gameplay rate (Playable Eileen reads it); [0x5D5248] = n at boot, the governor then sets 30
    }
    PokeBranch(0x414F0E, 0xE8, hkFpsStore, 5);
    PokeBranch(0x415202, 0xE8, hkFrameEnd, 5);
    PokeBranch(0x4184DC, 0xE9, hkAnim1, 5);
    PokeBranch(0x4192B5, 0xE9, hkAnim2, 5);
    PokeBranch(0x41848E, 0xE9, hkAnim3, 7);
    {  // v2.1 MENUS/CUTSCENES: switch in the tick of a phase change (optional: the frame end still governs without them)
        static const Site kPhase2 = { 0x4EFB70, { 0xC7, 0x05, 0x20, 0x5C, 0xFD, 0x00, 0x02, 0x00, 0x00, 0x00, 0xC3 }, 11 };
        static const Site kPhase0 = { 0x4EFB1E, { 0xE9, 0xCD, 0x41, 0x06, 0x00 }, 5 };  // jmp 553CF0
        static const Site kPhase1 = { 0x4EFB69, { 0xE9, 0x82, 0x41, 0x06, 0x00 }, 5 };  // jmp 553CF0
        if (SiteOk(&kPhase2) && SiteOk(&kPhase0) && SiteOk(&kPhase1)) {
            PokeBranch(0x4EFB70, 0xE9, hkPhase2, 11);
            PokeBranch(0x4EFB1E, 0xE9, hkPhaseTail, 5);
            PokeBranch(0x4EFB69, 0xE9, hkPhaseTail, 5);
        } else {
            Log("frame rate: the game phase setters 004EFB70 / 004EFB1E / 004EFB69 are not stock: rate changes wait for the frame end");
        }
    }
    g_rateHooks = 1;
    g_rateNow = n;  // what 0x403169 will store; the first frame end puts the front end at 30
    if (g_frameMode)
        Log("frame rate: FrameMode=1: the game runs at 30; the Options row picks the display rate (%d now)", kFps[g_fpsSel]);
    else
        Log("frame rate: gameplay %d%s; front end and in-game menus 30; live changes from the Options row", n,
            n != 30 ? " (EXPERIMENTAL)" : "");
    if (!g_frameMode && g_rateProbe)
        Log("frame rate: RateProbe=1 (diagnostic): the game loop stays at 30 ticks a second while the engine is told %d: "
            "everything that follows the rate runs in slow motion, per-tick leftovers at normal speed", n);
}

// ---------------------------------------------------------------- v1.7: the D3D device on the chosen monitor
// The game creates its device once (0x402F20): Direct3DCreate8 (`call 5AD4B2` at 0x402F66, the import thunk), then
// GetAdapterCount, CheckDeviceFormat, GetAdapterDisplayMode and CreateDevice (0x40317E), always with adapter 0 (esi = 0).
// GOG's wrapper (ori_d3d8.dll / d3d8.dll; IDirect3D8 vtable at RVA 120000) passes the adapter number straight to its
// IDirect3D9 (GetAdapterCount, GetAdapterMonitor, GetAdapterDisplayMode, CheckDevice*, and CreateDevice: the device
// object 2BD0 hands the game's adapter to IDirect3D9::CreateDevice at 30F9), so the device lands on the adapter the
// game names. The call at 0x402F66 is wrapped: after Direct3DCreate8 the adapter whose GetAdapterMonitor is the chosen
// monitor is looked up, and if it is not adapter 0 the methods that take an adapter (6..13 and CreateDevice 15) read
// "adapter 0" as that one. CreateDevice falls back to adapter 0 if the chosen adapter fails. The device is made once
// per run: a monitor chosen later moves the window (windowed presentation works across adapters); the adapter follows
// at the next start. Only with two or more monitors; the vtable is the wrapper's own, written per entry.
static void *g_d3dOrig[16];
static volatile LONG g_d3dAdapter;  // the chosen monitor's adapter (0 = nothing remapped)
static int g_d3dHooked;

#define D3D_ADAPTER_THUNK(i)                                                                        \
    static __declspec(naked) void D3dAdapter##i(void)                                              \
    {                                                                                               \
        __asm cmp dword ptr [esp + 8], 0                                                            \
        __asm jne pass                                                                              \
        __asm push eax                                                                              \
        __asm mov eax, g_d3dAdapter                                                                 \
        __asm mov dword ptr [esp + 12], eax                                                         \
        __asm pop eax                                                                               \
        __asm pass:                                                                                 \
        __asm jmp dword ptr [g_d3dOrig + (i) * 4]                                                   \
    }
D3D_ADAPTER_THUNK(6)
D3D_ADAPTER_THUNK(7)
D3D_ADAPTER_THUNK(8)
D3D_ADAPTER_THUNK(9)
D3D_ADAPTER_THUNK(10)
D3D_ADAPTER_THUNK(11)
D3D_ADAPTER_THUNK(12)
D3D_ADAPTER_THUNK(13)

typedef HRESULT(STDMETHODCALLTYPE *D3DCreateDevice_t)(void *, UINT, DWORD, HWND, DWORD, void *, void **);
static HRESULT STDMETHODCALLTYPE D3dCreateDevice(void *d3d, UINT adapter, DWORD type, HWND focus, DWORD flags, void *pp, void **dev)
{
    const D3DCreateDevice_t orig = (D3DCreateDevice_t)g_d3dOrig[15];
    if (adapter == 0 && g_d3dAdapter > 0) {
        HRESULT hr = orig(d3d, (UINT)g_d3dAdapter, type, focus, flags, pp, dev);
        Log("monitor: CreateDevice on adapter %ld (%s): %08lX%s", g_d3dAdapter, MonLabel(g_monSel), hr,
            SUCCEEDED(hr) ? "" : "; trying adapter 0");
        if (SUCCEEDED(hr)) return hr;
        InterlockedExchange(&g_d3dAdapter, 0);  // everything else asks adapter 0 again too
    }
    return orig(d3d, adapter, type, focus, flags, pp, dev);
}

static void SetVtbl(void **slot, void *fn)
{
    DWORD old;
    if (VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) {
        *slot = fn;
        VirtualProtect(slot, sizeof(void *), old, &old);
    }
}

static void D3dAdapterHook(void *d3d)
{
    static void *const kThunks[16] = { 0, 0, 0, 0, 0, 0, (void *)D3dAdapter6, (void *)D3dAdapter7, (void *)D3dAdapter8,
                                       (void *)D3dAdapter9, (void *)D3dAdapter10, (void *)D3dAdapter11, (void *)D3dAdapter12,
                                       (void *)D3dAdapter13, 0, (void *)D3dCreateDevice };
    void **vt = *(void ***)d3d;
    UINT(STDMETHODCALLTYPE *count)(void *) = (UINT(STDMETHODCALLTYPE *)(void *))vt[4];
    HMONITOR(STDMETHODCALLTYPE *mon)(void *, UINT) = (HMONITOR(STDMETHODCALLTYPE *)(void *, UINT))vt[14];
    HMONITOR want = MonGet(g_monSel, NULL);
    UINT n, k;
    int found = -1, i;
    if (g_d3dHooked) return;
    g_d3dHooked = 1;
    n = count(d3d);
    for (k = 0; k < n && k < 16 && found < 0; ++k)
        if (mon(d3d, k) == want) found = (int)k;
    Log("monitor: %u D3D adapter(s); %s is adapter %d%s", n, MonLabel(g_monSel), found,
        found > 0 ? ": the device is made there" : found == 0 ? " (the game's own choice)" : " (not found: adapter 0)");
    if (found <= 0) return;
    for (i = 6; i < 16; ++i)
        if (kThunks[i]) g_d3dOrig[i] = vt[i];
    InterlockedExchange(&g_d3dAdapter, found);
    MemoryBarrier();
    for (i = 6; i < 16; ++i)
        if (kThunks[i]) SetVtbl(&vt[i], kThunks[i]);
}

// 0x402F66 (was `call 5AD4B2`, stdcall Direct3DCreate8(SDKVersion)): the wrapper is loaded now, so its desktop size and
// its adapter list can be read.
static void *WINAPI GameD3DCreate8(UINT sdk)
{
    void *d3d = (*(void *(WINAPI **)(UINT))0x5B7244)(sdk);  // the exe's d3d8 import slot (bound by now)
    WrapperDesktop();
    if (d3d) D3dAdapterHook(d3d);
    return d3d;
}

static void ApplyD3DMonitor(void)
{
    static const Site kSite = { 0x402F66, { 0xE8, 0x47, 0xA5, 0x1A, 0x00 }, 5 };  // call 5AD4B2 (Direct3DCreate8)
    if (!g_enable || g_monN < 2) return;  // one monitor: nothing to choose; Enable=0: the game's own window and device
    if (!SiteOk(&kSite) || *(const WORD *)0x5AD4B2 != 0x25FF || *(const DWORD *)0x5AD4B4 != 0x5B7244) {
        Log("monitor: unexpected bytes at 00402F66 / 005AD4B2: the D3D device stays on adapter 0");
        return;
    }
    PokeBranch(0x402F66, 0xE8, GameD3DCreate8, 5);
}

// The stale fullscreen flag [0x6e7da4] minimised the game whenever it lost focus (also in full screen). Accepts the
// stock bytes (patched here, listed) or bytes already patched (an exe patched on disk, or the Playable Eileen plugin
// up to v0.81: not ours, not listed).
typedef struct { DWORD va; const char *name; BYTE stock[4], fixed[4]; int len; int *on; } ExeFix;
static const ExeFix kExeFixes[] = {
    { 0x415040, "no_minimize", { 0x74, 0x26 }, { 0xEB, 0x26 }, 2, &g_noMinimize },
};

static void ApplyExeFixes(void)
{
    int i;
    for (i = 0; i < (int)(sizeof kExeFixes / sizeof kExeFixes[0]); ++i) {
        const ExeFix *f = &kExeFixes[i];
        BYTE *p = (BYTE *)(ULONG_PTR)f->va;
        DWORD old;
        if (!*f->on) continue;
        if (!memcmp(p, f->fixed, f->len)) {
            Log("exe fix %s: already applied (exe file or another mod), left alone", f->name);
        } else if (!memcmp(p, f->stock, f->len)) {
            if (VirtualProtect(p, f->len, PAGE_EXECUTE_READWRITE, &old)) {
                memcpy(p, f->fixed, f->len);
                VirtualProtect(p, f->len, old, &old);
                FlushInstructionCache(GetCurrentProcess(), p, f->len);
                ListPatch(f->va, f->len);
                Log("exe fix %s: applied in memory at %08lX", f->name, f->va);
            } else {
                Log("exe fix %s: VirtualProtect failed (error %lu)", f->name, GetLastError());
            }
        } else {
            Log("exe fix %s: unexpected bytes %02X %02X %02X %02X at %08lX, left alone", f->name, p[0], p[1], p[2], p[3], f->va);
        }
    }
}

// ================================================================ window (from window.c)

static int g_userSizing, g_selfPlacing;
// v1.1: the outer size PlaceWindow asks for, and whether the user has sized the window since (drag, maximise, restore)
static volatile int g_placeW, g_placeH, g_userSized;
static volatile LONG g_vetoLogs;
static volatile DWORD g_attachTick;
static volatile int g_confined;
static HWND g_hwnd;
static WNDPROC g_origProc;
static DWORD g_style, g_winStyle;  // the frame the window has now; the windowed frame (v1.5)
static int g_dmCur = DM_WINDOWED, g_winSaved, g_winZoomed;  // v1.5 (Watch thread): the window's mode, the windowed rect
static RECT g_winRect;
static HMODULE g_exe;

typedef HWND (WINAPI *CreateWindowExA_t)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL (WINAPI *ShowWindow_t)(HWND, int);
typedef LONG (WINAPI *SetWindowLongA_t)(HWND, int, LONG);
typedef BOOL (WINAPI *SetCursorPos_t)(int, int);
typedef HRESULT (WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, void *);
typedef HRESULT (STDMETHODCALLTYPE *CreateDevice_t)(void *, REFGUID, void **, void *);
typedef HRESULT (STDMETHODCALLTYPE *SetCoopLevel_t)(void *, HWND, DWORD);
static CreateWindowExA_t oCreateWindowExA;
static ShowWindow_t oShowWindow;
static SetWindowLongA_t oSetWindowLongA;
static SetCursorPos_t oSetCursorPos;
static DirectInput8Create_t oDirectInput8Create;
static volatile LONG g_diHooked;

static int InFront(void) { return g_hwnd && GetForegroundWindow() == g_hwnd; }

static void Confine(int on)
{
    if (on) {
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        MapWindowPoints(g_hwnd, NULL, (POINT *)&rc, 2);
        ClipCursor(&rc);
    } else {
        ClipCursor(NULL);
    }
    g_confined = on;
}

static void OuterSize(int *w, int *h)
{
    RECT r = { 0, 0, 0, 0 };
    r.right = g_cliW;
    r.bottom = g_cliH;
    AdjustWindowRect(&r, g_style, FALSE);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}

// Centred on the chosen monitor (v1.7; the primary screen before, which is the same rect when the primary is chosen)
static void PlaceWindow(HWND hwnd)
{
    MONITORINFOEXA mi;
    int w, h;
    const LONG sel = g_monSel;
    MonGet(sel, &mi);
    OuterSize(&w, &h);
    g_placeW = w;
    g_placeH = h;
    oSetWindowLongA(hwnd, GWL_STYLE, g_style | (GetWindowLongA(hwnd, GWL_STYLE) & WS_VISIBLE));
    oSetWindowLongA(hwnd, GWL_EXSTYLE, GetWindowLongA(hwnd, GWL_EXSTYLE) & ~WS_EX_TOPMOST);
    g_selfPlacing = 1;
    SetWindowPos(hwnd, HWND_NOTOPMOST, mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.left - w) / 2,
                 mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.top - h) / 2, w, h, SWP_FRAMECHANGED | SWP_NOACTIVATE);
    g_selfPlacing = 0;
    g_monPlaced = sel;
}

// Drag-resizing snaps the client area to standard heights in the picture's shape (v1.3; 4:3 before the game picked a
// size), plus the ini height, that fit the monitor's work area. The dragged edge decides which dimension is matched;
// the opposite edges stay put.
static const int kStdHeights[] = { 480, 540, 600, 720, 768, 800, 864, 900, 960, 1050, 1080, 1200, 1440, 1536, 1600, 1800, 2160, 2400 };

static int ShapeWidth(int h)  // client width for height h in the picture's shape
{
    LONG pw = g_picW, ph = g_picH;
    return pw > 0 && ph > 0 ? MulDiv(h, pw, ph) : h * 4 / 3;
}

static void SnapSizing(HWND hwnd, int edge, RECT *r)
{
    RECT frame = { 0, 0, 0, 0 };
    MONITORINFO mi;
    int fw, fh, cw, ch, i, n = sizeof kStdHeights / sizeof kStdHeights[0], bestW = 0, bestH = 0, bestD = 0x7fffffff;
    mi.cbSize = sizeof mi;
    AdjustWindowRect(&frame, (DWORD)GetWindowLongA(hwnd, GWL_STYLE), FALSE);
    fw = frame.right - frame.left;
    fh = frame.bottom - frame.top;
    cw = r->right - r->left - fw;
    ch = r->bottom - r->top - fh;
    if (!GetMonitorInfoA(MonitorFromRect(r, MONITOR_DEFAULTTONEAREST), &mi)) return;
    for (i = -1; i < n; ++i) {
        int h = i < 0 ? g_height : kStdHeights[i], w = ShapeWidth(h), d;
        if (w + fw > mi.rcWork.right - mi.rcWork.left || h + fh > mi.rcWork.bottom - mi.rcWork.top) continue;
        if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT) d = abs(w - cw);
        else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) d = abs(h - ch);
        else d = abs(w - cw) + abs(h - ch);
        if (d < bestD) { bestD = d; bestW = w; bestH = h; }
    }
    if (bestD == 0x7fffffff) return;  // nothing fits: leave the drag alone
    if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT) r->left = r->right - (bestW + fw);
    else r->right = r->left + bestW + fw;
    if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT) r->top = r->bottom - (bestH + fh);
    else r->bottom = r->top + bestH + fh;
}

// v1.3: the largest client in the picture's shape inside a work area of areaW x areaH (frame = the window border).
static void FitShape(int frameW, int frameH, int areaW, int areaH, int *cw, int *ch)
{
    LONG pw = g_picW, ph = g_picH;
    *ch = areaH - frameH;
    *cw = ShapeWidth(*ch);
    if (*cw > areaW - frameW) {
        *cw = areaW - frameW;
        *ch = pw > 0 && ph > 0 ? MulDiv(*cw, ph, pw) : *cw * 3 / 4;
    }
}

// v1.5 Borderless: a popup over the whole monitor (the wrapper's back buffer is the desktop; it letterboxes the picture).
// v1.7: the chosen monitor.
static void FillMonitor(void)
{
    MONITORINFOEXA mi;
    RECT wr;
    const LONG sel = g_monSel;
    if (g_monN) {
        MonGet(sel, &mi);
        g_monPlaced = sel;
    } else {
        mi.cbSize = sizeof mi;
        if (!GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), (MONITORINFO *)&mi)) return;
    }
    if (GetWindowRect(g_hwnd, &wr) && EqualRect(&wr, &mi.rcMonitor)) return;
    g_placeW = mi.rcMonitor.right - mi.rcMonitor.left;
    g_placeH = mi.rcMonitor.bottom - mi.rcMonitor.top;
    g_selfPlacing = 1;
    SetWindowPos(g_hwnd, NULL, mi.rcMonitor.left, mi.rcMonitor.top, g_placeW, g_placeH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    g_selfPlacing = 0;
    {  // v3.41 VW341: the popup's client must be the whole monitor, exactly (the wrapper's back buffer is that size)
        RECT cr = { 0, 0, 0, 0 };
        POINT o = { 0, 0 };
        const int got = GetClientRect(g_hwnd, &cr) && ClientToScreen(g_hwnd, &o);
        if (got && o.x == mi.rcMonitor.left && o.y == mi.rcMonitor.top && cr.right == g_placeW && cr.bottom == g_placeH)
            Log("window: borderless %dx%d at %ld,%ld (client %ldx%ld: the whole monitor)", g_placeW, g_placeH, mi.rcMonitor.left,
                mi.rcMonitor.top, cr.right, cr.bottom);
        else
            LogAnomaly("window", "borderless %dx%d at %ld,%ld: the client is %ldx%ld at %ld,%ld, not the whole monitor", g_placeW,
                       g_placeH, mi.rcMonitor.left, mi.rcMonitor.top, cr.right, cr.bottom, o.x, o.y);
    }
}

// v1.7 (Watch thread, windowed): to the chosen monitor, centred in its work area, same client size if it fits (else
// the largest of the picture's shape); a maximised window is maximised again there.
static void MoveToMonitor(void)
{
    MONITORINFOEXA mi;
    RECT cr, frame = { 0, 0, 0, 0 };
    int zoomed, fw, fh, cw, ch, aw, ah, x, y;
    const LONG sel = g_monSel;
    HMONITOR hm = MonGet(sel, &mi);
    g_monPlaced = sel;
    if (!g_hwnd || IsIconic(g_hwnd) || MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST) == hm) return;
    zoomed = IsZoomed(g_hwnd);
    if (zoomed) {
        g_userSizing = 1;
        ShowWindow(g_hwnd, SW_RESTORE);
        g_userSizing = 0;
    }
    if (!GetClientRect(g_hwnd, &cr)) return;
    AdjustWindowRect(&frame, g_style, FALSE);
    fw = frame.right - frame.left;
    fh = frame.bottom - frame.top;
    aw = mi.rcWork.right - mi.rcWork.left;
    ah = mi.rcWork.bottom - mi.rcWork.top;
    cw = cr.right > 0 ? cr.right : g_cliW;
    ch = cr.bottom > 0 ? cr.bottom : g_cliH;
    if (cw + fw > aw || ch + fh > ah) FitShape(fw, fh, aw, ah, &cw, &ch);
    x = mi.rcWork.left + (aw - (cw + fw)) / 2;
    y = mi.rcWork.top + (ah - (ch + fh)) / 2;
    g_cliW = cw;
    g_cliH = ch;
    g_placeW = cw + fw;
    g_placeH = ch + fh;
    g_selfPlacing = 1;
    SetWindowPos(g_hwnd, NULL, x, y, cw + fw, ch + fh, SWP_NOZORDER | SWP_NOACTIVATE);
    g_selfPlacing = 0;
    if (zoomed) {
        g_userSizing = 1;
        ShowWindow(g_hwnd, SW_MAXIMIZE);
        g_userSizing = 0;
    }
    if (g_confined) Confine(1);
    Log("window: moved to %s, client %dx%d at %d,%d", MonLabel(sel), cw, ch, x, y);
}

// v1.7 (game thread, the end of a drag): a windowed game dragged onto another monitor makes that the chosen one, kept
// in the Options byte (live and the menu's backup, so the menu sees no change) and in the ini.
static void AdoptWindowMonitor(HWND hwnd)
{
    int k;
    if (g_dmCur != DM_WINDOWED || g_monN < 2) return;
    k = MonIndexOf(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
    if (k < 0 || k == g_monSel) return;
    g_monPlaced = k;
    if (g_monRow) {
        CFG[0x23] = (BYTE)(k + 1);
        CFG[0x24 + 0x23] = (BYTE)(k + 1);  // 0x1091AF4 + 23h: the copy the menu compares with
        g_monApplied = k;
    }
    SetMonitor(k, "the window was dragged there");
}

// v1.5: frame and place for the mode. Going borderless remembers the window (and whether it was maximised); going back
// puts it there again, then ReshapeWindow gives it the picture's shape as usual.
static void SwitchWindowMode(int m)
{
    if (m == DM_BORDERLESS) {
        g_winZoomed = IsZoomed(g_hwnd);
        if (g_winZoomed) {
            g_userSizing = 1;
            ShowWindow(g_hwnd, SW_RESTORE);
            g_userSizing = 0;
        }
        g_winSaved = GetWindowRect(g_hwnd, &g_winRect);
        g_style = WS_POPUP;
    } else {
        g_style = g_winStyle;
    }
    g_dmCur = m;
    oSetWindowLongA(g_hwnd, GWL_STYLE, g_style | (GetWindowLongA(g_hwnd, GWL_STYLE) & WS_VISIBLE));
    if (m == DM_BORDERLESS) {
        FillMonitor();
    } else if (g_winSaved) {
        MONITORINFOEXA mi;
        const LONG sel = g_monSel;
        g_placeW = g_winRect.right - g_winRect.left;
        g_placeH = g_winRect.bottom - g_winRect.top;
        if (g_monN && MonGet(sel, &mi) != MonitorFromRect(&g_winRect, MONITOR_DEFAULTTONEAREST)) {
            // v1.7: the monitor changed while borderless: the same window, centred on the chosen one (ReshapeWindow
            // then fits it into that work area)
            OffsetRect(&g_winRect, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - g_placeW) / 2 - g_winRect.left,
                       mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - g_placeH) / 2 - g_winRect.top);
            if (g_winRect.left < mi.rcWork.left) OffsetRect(&g_winRect, mi.rcWork.left - g_winRect.left, 0);
            if (g_winRect.top < mi.rcWork.top) OffsetRect(&g_winRect, 0, mi.rcWork.top - g_winRect.top);
        }
        g_monPlaced = sel;
        g_selfPlacing = 1;
        SetWindowPos(g_hwnd, NULL, g_winRect.left, g_winRect.top, g_placeW, g_placeH, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        g_selfPlacing = 0;
        if (g_winZoomed) {
            g_userSizing = 1;
            ShowWindow(g_hwnd, SW_MAXIMIZE);
            g_userSizing = 0;
        }
    } else {
        PlaceWindow(g_hwnd);
    }
    if (g_confined) Confine(1);
    Log("window: %s", kDmName[m]);
}

// v1.3 (Watch thread, after the game picked a size): the window keeps its height and its centre and takes the
// picture's shape, so the wrapper's stretch of the game picture over the client is uniform. A maximised window is
// restored and maximised again (WM_GETMINMAXINFO gives it the shape).
static void ReshapeWindow(void)
{
    RECT cr, wr, frame = { 0, 0, 0, 0 };
    MONITORINFO mi;
    int zoomed, cw, ch, fw, fh, aw, ah, x, y;
    if (!g_hwnd || g_picW <= 0 || g_picH <= 0 || IsIconic(g_hwnd)) return;
    if (g_dm != g_dmCur) SwitchWindowMode(g_dm);  // v1.5
    if (g_dmCur == DM_BORDERLESS) {
        FillMonitor();  // v1.7: on the chosen monitor
        return;
    }
    if (g_monN && g_monPlaced != g_monSel) MoveToMonitor();  // v1.7
    zoomed = IsZoomed(g_hwnd);
    if (zoomed) {
        g_userSizing = 1;
        ShowWindow(g_hwnd, SW_RESTORE);
        g_userSizing = 0;
    }
    mi.cbSize = sizeof mi;
    if (!GetClientRect(g_hwnd, &cr) || !GetWindowRect(g_hwnd, &wr) ||
        !GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        return;
    AdjustWindowRect(&frame, g_style, FALSE);
    fw = frame.right - frame.left;
    fh = frame.bottom - frame.top;
    aw = mi.rcWork.right - mi.rcWork.left;
    ah = mi.rcWork.bottom - mi.rcWork.top;
    ch = cr.bottom > 0 ? cr.bottom : g_cliH;
    cw = ShapeWidth(ch);
    if (cw + fw > aw || ch + fh > ah) FitShape(fw, fh, aw, ah, &cw, &ch);
    if (abs(cw - cr.right) > 1 || ch != cr.bottom) {
        x = (wr.left + wr.right) / 2 - (cw + fw) / 2;
        y = (wr.top + wr.bottom) / 2 - (ch + fh) / 2;
        if (x + cw + fw > mi.rcWork.right) x = mi.rcWork.right - (cw + fw);
        if (y + ch + fh > mi.rcWork.bottom) y = mi.rcWork.bottom - (ch + fh);
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
        g_cliW = cw;
        g_cliH = ch;
        g_placeW = cw + fw;
        g_placeH = ch + fh;
        g_selfPlacing = 1;
        SetWindowPos(g_hwnd, NULL, x, y, cw + fw, ch + fh, SWP_NOZORDER | SWP_NOACTIVATE);
        g_selfPlacing = 0;
        Log("window: client %ldx%ld -> %dx%d (the picture's shape, game size %ldx%ld)", cr.right, cr.bottom, cw, ch, g_picW, g_picH);
    }
    if (zoomed) {
        g_userSizing = 1;
        ShowWindow(g_hwnd, SW_MAXIMIZE);
        g_userSizing = 0;
    }
}

static LRESULT CALLBACK HookProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZING:
        if (g_snap) {
            SnapSizing(hwnd, (int)wp, (RECT *)lp);
            return TRUE;
        }
        break;
    case WM_STYLECHANGING:
        if (wp == GWL_STYLE) ((STYLESTRUCT *)lp)->styleNew = g_style | (((STYLESTRUCT *)lp)->styleNew & WS_VISIBLE);
        else if (wp == (WPARAM)GWL_EXSTYLE) ((STYLESTRUCT *)lp)->styleNew &= ~WS_EX_TOPMOST;
        break;
    case WM_WINDOWPOSCHANGING: {
        // the game and the wrapper try to stretch the window over the screen; only the user (drag / caption buttons)
        // or PlaceWindow may move or size it. v1.1: PlaceWindow's own size is recognised by the size it asks for, not by
        // g_selfPlacing alone: when the Watch thread attaches, g_selfPlacing is set on that thread while the game thread
        // may be sizing the window itself (the GOG wrapper's full-screen size), and that size used to slip through.
        WINDOWPOS *wpos = (WINDOWPOS *)lp;
        if (!g_userSizing && (wpos->flags & (SWP_NOMOVE | SWP_NOSIZE)) != (SWP_NOMOVE | SWP_NOSIZE)) {
            const int ours = g_selfPlacing && ((wpos->flags & SWP_NOSIZE) || (wpos->cx == g_placeW && wpos->cy == g_placeH));
            if (!ours) {
                if (!(wpos->flags & SWP_NOSIZE) && InterlockedIncrement(&g_vetoLogs) <= 6)
                    Log("window: refused a resize to %dx%d (thread %lu)", wpos->cx, wpos->cy, GetCurrentThreadId());
                wpos->flags |= SWP_NOMOVE | SWP_NOSIZE;
            }
        }
        if (!(wpos->flags & SWP_NOZORDER) && wpos->hwndInsertAfter == HWND_TOPMOST) wpos->hwndInsertAfter = HWND_NOTOPMOST;
        break;
    }
    case WM_WINDOWPOSCHANGED:
        if (g_confined) Confine(1);  // follow the client area
        break;
    case WM_GETMINMAXINFO:
        if (g_picW > 0 && g_picH > 0 && g_dmCur != DM_BORDERLESS) {  // v1.3: maximised = the largest client in the picture's shape, centred
            LRESULT res = CallWindowProcA(g_origProc, hwnd, msg, wp, lp);
            MINMAXINFO *mm = (MINMAXINFO *)lp;
            MONITORINFO mi;
            RECT frame = { 0, 0, 0, 0 };
            int fw, fh, aw, ah, cw, ch;
            mi.cbSize = sizeof mi;
            if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
                AdjustWindowRect(&frame, g_style, FALSE);
                fw = frame.right - frame.left;
                fh = frame.bottom - frame.top;
                aw = mi.rcWork.right - mi.rcWork.left;
                ah = mi.rcWork.bottom - mi.rcWork.top;
                FitShape(fw, fh, aw, ah, &cw, &ch);
                mm->ptMaxSize.x = cw + fw;
                mm->ptMaxSize.y = ch + fh;
                mm->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left + (aw - (cw + fw)) / 2;
                mm->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top + (ah - (ch + fh)) / 2;
            }
            return res;
        }
        break;
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_MINIMIZE && g_keepActive) return 0;
        if ((wp & 0xFFF0) == SC_MAXIMIZE && g_dmCur == DM_BORDERLESS) return 0;
        if ((wp & 0xFFF0) == SC_MAXIMIZE || ((wp & 0xFFF0) == SC_RESTORE && IsZoomed(hwnd))) {
            LRESULT res;
            g_userSizing = 1;
            g_userSized = 1;
            res = CallWindowProcA(g_origProc, hwnd, msg, wp, lp);
            g_userSizing = 0;
            return res;
        }
        break;
    case WM_ENTERSIZEMOVE:
        g_userSizing = 1;
        if (g_confined) Confine(0);
        break;
    case WM_EXITSIZEMOVE:
        g_userSizing = 0;
        g_userSized = 1;
        AdoptWindowMonitor(hwnd);  // v1.7
        break;
    case WM_SETCURSOR:
        if (g_capture && g_confined && LOWORD(lp) == HTCLIENT) {
            SetCursor(NULL);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        // client-area messages only: capture once the pointer is back inside, so clicking the title bar to drag the
        // window still works
        if (g_capture && !g_confined && !g_userSizing && InFront()) {
            Confine(1);
            SetCursor(NULL);
        }
        break;
    case WM_ACTIVATEAPP:
        if (!wp) {
            if (g_confined) Confine(0);
            if (g_keepActive) return 0;  // the game never learns it lost focus: no minimize / pause
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) {
            if (g_confined) Confine(0);
            if (g_keepActive) return 0;
        }
        break;
    case WM_KILLFOCUS:
        if (g_confined) Confine(0);
        if (g_keepActive) return 0;
        break;
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED && g_keepActive) { oShowWindow(hwnd, SW_SHOWNOACTIVATE); return 0; }
        break;
    case WM_SYSKEYDOWN:  // v1.5: Alt+Enter switches Windowed / Borderless (kept in the Options like the menu row)
        if (wp == VK_RETURN && g_dispRow && g_dmApplied >= 0 && !(lp & 0x40000000)) {
            const int m = g_dm == DM_BORDERLESS ? DM_WINDOWED : DM_BORDERLESS;
            CFG[0x22] = (BYTE)(m + 1);
            CFG[0x24 + 0x22] = (BYTE)(m + 1);  // v2.0: the Options backup too (0x1091AF4), so "No" does not undo it
            g_dmApplied = m;
            SetDisplayMode(m);
            return 0;
        }
        break;
    case WM_SYSCHAR:
        if (wp == '\r' && g_dispRow) return 0;  // no beep for Alt+Enter
        break;
    }
    return CallWindowProcA(g_origProc, hwnd, msg, wp, lp);
}

// Veto minimizing the game window no matter who asks.
static LRESULT CALLBACK CbtProc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HCBT_MINMAX && (HWND)wp == g_hwnd && g_keepActive) {
        int cmd = LOWORD(lp);
        if (cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE || cmd == SW_FORCEMINIMIZE) return 1;
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

static void Attach(HWND hwnd)
{
    if (g_hwnd || !hwnd) return;
    g_hwnd = hwnd;
    g_origProc = (WNDPROC)oSetWindowLongA(hwnd, GWL_WNDPROC, (LONG)(LONG_PTR)HookProc);
    SetWindowsHookExA(WH_CBT, CbtProc, NULL, GetWindowThreadProcessId(hwnd, NULL));
    if (g_picW > 0) g_cliW = ShapeWidth(g_cliH);  // v1.3: the game picked its size before the window existed
    PlaceWindow(hwnd);
    if (g_dm != g_dmCur && g_reshapeEvt) SetEvent(g_reshapeEvt);  // v1.5: Borderless chosen before the window existed
    g_attachTick = GetTickCount();
    Log("window: attached %p, client %dx%d, resizable %d, snap %d, capture mouse %d, keep active %d", hwnd, g_cliW, g_cliH,
        g_resizable, g_snap, g_capture, g_keepActive);
}

static HWND WINAPI hkCreateWindowExA(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x, int y, int w, int h,
                                     HWND parent, HMENU menu, HINSTANCE inst, LPVOID param)
{
    HWND hwnd;
    if (parent || g_hwnd || (style & WS_CHILD)) return oCreateWindowExA(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    hwnd = oCreateWindowExA(ex & ~WS_EX_TOPMOST, cls, name, g_style | (style & WS_VISIBLE), CW_USEDEFAULT, CW_USEDEFAULT,
                            g_cliW, g_cliH, parent, menu, inst, param);
    Attach(hwnd);
    return hwnd;
}

static int IsMinimizeCmd(UINT cmd)
{
    return cmd == SW_MINIMIZE || cmd == SW_SHOWMINIMIZED || cmd == SW_SHOWMINNOACTIVE || cmd == SW_FORCEMINIMIZE || cmd == SW_HIDE;
}

static BOOL WINAPI hkShowWindow(HWND hwnd, int cmd)
{
    if (hwnd == g_hwnd && g_keepActive && IsMinimizeCmd((UINT)cmd)) return TRUE;
    if (hwnd == g_hwnd && cmd == SW_MAXIMIZE) cmd = SW_SHOWNORMAL;
    return oShowWindow(hwnd, cmd);
}

static LONG WINAPI hkSetWindowLongA(HWND hwnd, int idx, LONG val)
{
    if (hwnd == g_hwnd) {
        if (idx == GWL_STYLE) val = g_style | (val & WS_VISIBLE);
        else if (idx == GWL_EXSTYLE) val &= ~WS_EX_TOPMOST;
        else if (idx == GWL_WNDPROC) {  // keep our hook in front of whatever gets installed
            LONG prev = (LONG)(LONG_PTR)g_origProc;
            g_origProc = (WNDPROC)(LONG_PTR)val;
            return prev;
        }
    }
    return oSetWindowLongA(hwnd, idx, val);
}

static LONG WINAPI hkSetWindowLongW(HWND hwnd, int idx, LONG val)
{
    if (hwnd == g_hwnd) {
        if (idx == GWL_STYLE) val = g_style | (val & WS_VISIBLE);
        else if (idx == GWL_EXSTYLE) val &= ~WS_EX_TOPMOST;
    }
    return SetWindowLongW(hwnd, idx, val);
}

static BOOL WINAPI hkSetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *wp)
{
    if (hwnd == g_hwnd && g_keepActive && IsMinimizeCmd(wp->showCmd)) return TRUE;
    return SetWindowPlacement(hwnd, wp);
}

// The game and the wrapper poll these to decide whether to pause / minimize.
static HWND WINAPI hkGetForegroundWindow(void)
{
    HWND fg = GetForegroundWindow();
    return (g_keepActive && g_hwnd) ? g_hwnd : fg;
}

static HWND WINAPI hkGetActiveWindow(void)
{
    HWND act = GetActiveWindow();
    return (g_keepActive && g_hwnd) ? g_hwnd : act;
}

// The wrapper's own confinement is replaced by ours (Confine).
static BOOL WINAPI hkClipCursor(const RECT *rc)
{
    (void)rc;
    if (g_confined) { Confine(1); return TRUE; }
    return ClipCursor(NULL);
}

// The wrapper yanks its window back to the front whenever another app is clicked.
static BOOL WINAPI hkSetForegroundWindow(HWND hwnd)
{
    if (g_keepActive && hwnd == g_hwnd && GetForegroundWindow() != g_hwnd) return TRUE;
    return SetForegroundWindow(hwnd);
}

static BOOL WINAPI hkIsIconic(HWND hwnd)
{
    return (g_keepActive && hwnd == g_hwnd) ? FALSE : IsIconic(hwnd);
}

// The game recenters the cursor every frame; only while it owns the cursor.
static BOOL WINAPI hkSetCursorPos(int x, int y)
{
    POINT pt;
    RECT rc;
    if (g_hwnd) {
        if (!InFront() || !GetCursorPos(&pt)) return TRUE;
        GetClientRect(g_hwnd, &rc);
        MapWindowPoints(g_hwnd, NULL, (POINT *)&rc, 2);
        if (!PtInRect(&rc, pt)) return TRUE;
    }
    return oSetCursorPos(x, y);
}

// ---- DirectInput: non-exclusive cooperative level. One original per vtable (SH4Windowed.asi 2026-10-19): device
// objects of different kinds (keyboard, mouse, a gamepad through GOG's dinput8.dll) can have different vtables, and a
// single shared original let a controller connected mid-game redirect the keyboard's calls (crash on connect).
#define MAX_VT 32
typedef struct { void **vtbl; void *orig; } VtOrig;
static VtOrig g_coopOrig[MAX_VT], g_createOrig[MAX_VT];
static volatile LONG g_vtLock;

static void *FindOrig(VtOrig *t, void **vtbl)
{
    int i;
    for (i = 0; i < MAX_VT; i++)
        if (t[i].vtbl == vtbl) return t[i].orig;
    return NULL;
}

static void PatchVtbl(VtOrig *t, void *obj, int idx, void *hook)
{
    void **vtbl = *(void ***)obj;
    DWORD old;
    int i;
    while (InterlockedExchange(&g_vtLock, 1)) Sleep(0);
    if (vtbl[idx] != hook && !FindOrig(t, vtbl)) {
        for (i = 0; i < MAX_VT && t[i].vtbl; i++) {}
        if (i < MAX_VT) {  // table full: leave this vtable unhooked (safe)
            t[i].orig = vtbl[idx];
            MemoryBarrier();
            t[i].vtbl = vtbl;  // published before the hook can be reached
            if (VirtualProtect(&vtbl[idx], sizeof(void *), PAGE_READWRITE, &old)) {
                vtbl[idx] = hook;
                VirtualProtect(&vtbl[idx], sizeof(void *), old, &old);
            }
        }
    }
    InterlockedExchange(&g_vtLock, 0);
}

// DISCL_EXCLUSIVE (1) -> DISCL_NONEXCLUSIVE (2) so input isn't grabbed from Windows.
static HRESULT STDMETHODCALLTYPE hkSetCoopLevel(void *dev, HWND hwnd, DWORD flags)
{
    SetCoopLevel_t orig = (SetCoopLevel_t)FindOrig(g_coopOrig, *(void ***)dev);
    if (!orig) return E_FAIL;  // never expected: the hook is only placed with its original
    if (flags & 1) flags = (flags & ~1u) | 2;
    return orig(dev, hwnd, flags);
}

// v3.14.0: IDirectInputDevice8::SendForceFeedbackCommand (slot 22). GOG's wrapper calls it on its own device every 100 ms
// with 0x80000000 | v << 8 | v (v = 32 / 33 at the game's strength): Medium / High scale both motor bytes. Runs on the
// wrapper's thread: arithmetic and a short log only.
typedef HRESULT(STDMETHODCALLTYPE *SendFF_t)(void *, DWORD);
static VtOrig g_ffOrig[MAX_VT];
static HRESULT STDMETHODCALLTYPE hkSendFF(void *dev, DWORD cmd)
{
    static DWORD s_last = 0xFFFFFFFF;
    static volatile LONG s_lines;
    SendFF_t orig = (SendFF_t)FindOrig(g_ffOrig, *(void ***)dev);
    DWORD out = cmd;
    if (!orig) return E_FAIL;
    if ((cmd & 0xB0000000u) == 0x80000000u) {
        const LONG lvl = g_vibLevel, cap = g_vibCap;
        if (lvl >= 2) {
            // review VIB-003: the targets are fractions of the cap (Medium half, High all of it), so the two levels
            // stay apart on a 0..100 pad too (128 / 255, or 50 / 100); 33 -> T, 32 -> T * 32 / 33 (still distinct)
            const DWORD T = lvl == 2 ? (DWORD)cap * 128u / 255u : (DWORD)cap;
            DWORD b[2], k;
            b[0] = cmd & 0xFF;
            b[1] = (cmd >> 8) & 0xFF;
            for (k = 0; k < 2; ++k) {
                DWORD s = b[k] * T / 33u;
                b[k] = s > T ? T : s;  // a stock byte above 33 (never seen) is held at the target
            }
            out = (cmd & 0xFFFF0000u) | (b[1] << 8) | b[0];
        }
        {
            const int on = (out & 0xFFFF) != 0, was = s_last != 0xFFFFFFFF && (s_last & 0xFFFF) != 0;
            if (s_lines < 60 && (on != was || g_vibLogN > 0)) {
            if (g_vibLogN > 0) InterlockedDecrement(&g_vibLogN);
            InterlockedIncrement(&s_lines);
                Log("vibration: command %08lX -> %08lX (%s, cap %ld) at %lu ms", cmd, out, kVibName[lvl], cap, GetTickCount());
            }
        }
        s_last = out;
    }
    return orig(dev, out);
}

static HRESULT STDMETHODCALLTYPE hkCreateDevice(void *di, REFGUID guid, void **dev, void *outer)
{
    CreateDevice_t orig = (CreateDevice_t)FindOrig(g_createOrig, *(void ***)di);
    HRESULT hr;
    if (!orig) return E_FAIL;
    hr = orig(di, guid, dev, outer);
    if (SUCCEEDED(hr) && dev && *dev) {
        if (g_enable) PatchVtbl(g_coopOrig, *dev, 13, hkSetCoopLevel);
        if (g_vibOn) {  // v3.14.0
            InterlockedExchange(&g_vibCap, VibCapNow());
            PatchVtbl(g_ffOrig, *dev, 22, hkSendFF);
        }
    }
    return hr;
}

static HRESULT WINAPI hkDirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID *out, void *outer)
{
    HRESULT hr = oDirectInput8Create(inst, ver, riid, out, outer);
    if (SUCCEEDED(hr) && out && *out) PatchVtbl(g_createOrig, *out, 3, hkCreateDevice);
    return hr;
}

// ---- GOG dinput8.dll HID reads (SH4Windowed.asi hidfix2, 2026-10-20; crash_2026-10-19_2244.md / _2026-10-20_0857.md)
// GOG's input wrapper reads each HID controller on its own thread: an overlapped ReadFile into a buffer on that
// thread's stack, and on two stop paths the thread exits with the read still pending, so a late report (a DualSense
// streams one every few ms) lands in a dead stack frame. Overlapped reads into the calling thread's stack go through a
// per-thread heap bounce buffer instead and are copied back when they complete; an abandoned read can then only write
// into heap memory that is never freed while the read may be pending. Closing the read's event (or NULL) with the read
// pending cancels it.
typedef struct { BYTE *bounce; DWORD size; void *dest; OVERLAPPED *ov; HANDLE file; int pending; } StackRead;
static DWORD g_tlsRead = TLS_OUT_OF_INDEXES;

// v1.9 (audit CMP-5): GetCurrentThreadStackLimits is Windows 8+; a static import kept the dll (and so the game) from
// loading on Windows 7. Looked up at run time; without it the TEB's StackLimit / StackBase (committed part) are used.
typedef VOID(WINAPI *StackLimits_t)(PULONG_PTR, PULONG_PTR);
static int OnMyStack(const void *p)
{
    static StackLimits_t fn;
    static int looked;
    ULONG_PTR lo, hi;
    if (!looked) {
        fn = (StackLimits_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetCurrentThreadStackLimits");
        looked = 1;
    }
    if (fn) {
        fn(&lo, &hi);
    } else {
        NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
        lo = (ULONG_PTR)tib->StackLimit;
        hi = (ULONG_PTR)tib->StackBase;
    }
    return (ULONG_PTR)p >= lo && (ULONG_PTR)p < hi;
}

static BOOL WINAPI hkWrapReadFile(HANDLE h, LPVOID buf, DWORD n, LPDWORD got, LPOVERLAPPED ov)
{
    StackRead *r;
    BOOL ok;
    DWORD err;
    if (!ov || !n || !OnMyStack(buf)) return ReadFile(h, buf, n, got, ov);
    if ((r = (StackRead *)TlsGetValue(g_tlsRead)) == NULL) {
        if ((r = (StackRead *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *r)) == NULL) return ReadFile(h, buf, n, got, ov);
        TlsSetValue(g_tlsRead, r);
    }
    if (r->pending || r->size < n) {
        if (!r->pending) HeapFree(GetProcessHeap(), 0, r->bounce);  // a pending one is left to the kernel (leaked)
        r->bounce = (BYTE *)HeapAlloc(GetProcessHeap(), 0, n);
        r->size = r->bounce ? n : 0;
        r->pending = 0;
        if (!r->bounce) return ReadFile(h, buf, n, got, ov);
    }
    r->dest = buf;
    r->ov = ov;
    r->file = h;
    ok = ReadFile(h, r->bounce, n, got, ov);
    err = GetLastError();  // the wrapper checks it for ERROR_IO_PENDING
    if (ok) {
        DWORD k = got ? *got : (DWORD)ov->InternalHigh;
        memcpy(buf, r->bounce, k < n ? k : n);
    } else if (err == ERROR_IO_PENDING) {
        r->pending = 1;
    }
    SetLastError(err);
    return ok;
}

static BOOL WINAPI hkWrapGetOverlappedResult(HANDLE h, LPOVERLAPPED ov, LPDWORD got, BOOL wait)
{
    BOOL ok = GetOverlappedResult(h, ov, got, wait);
    DWORD err = GetLastError();
    StackRead *r = (StackRead *)TlsGetValue(g_tlsRead);
    if (r && r->pending && ov == r->ov && (ok || err != ERROR_IO_INCOMPLETE)) {
        if (ok && got) memcpy(r->dest, r->bounce, *got < r->size ? *got : r->size);
        r->pending = 0;  // finished: the bounce buffer may be reused
    }
    SetLastError(err);
    return ok;
}

static BOOL WINAPI hkWrapCloseHandle(HANDLE x)
{
    DWORD err = GetLastError();
    StackRead *r = (StackRead *)TlsGetValue(g_tlsRead);
    if (r && r->pending) {
        __try {
            if (!x || r->ov->hEvent == x) CancelIoEx(r->file, r->ov);  // the read is being abandoned
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }
    SetLastError(err);
    return CloseHandle(x);
}

// ---- import hooks

// Patches one import in a module's IAT; slots of the game exe are listed in SH4WM_Patches. Returns the slots patched.
static int PatchIat(HMODULE mod, const char *dll, const char *func, void *hook, void **orig)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *imp;
    HMODULE owner = GetModuleHandleA(dll);
    void *target = owner ? (void *)GetProcAddress(owner, func) : NULL;
    int n = 0;
    if (!dir.VirtualAddress || !target) return 0;
    if (orig && !*orig) *orig = target;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress); imp->Name; imp++) {
        IMAGE_THUNK_DATA *t;
        if (_stricmp((char *)(base + imp->Name), dll)) continue;
        for (t = (IMAGE_THUNK_DATA *)(base + imp->FirstThunk); t->u1.Function; t++) {
            if ((void *)t->u1.Function == target) {  // byte check: the slot still holds the real function
                DWORD old;
                if (!VirtualProtect(&t->u1.Function, sizeof(void *), PAGE_READWRITE, &old)) continue;
                t->u1.Function = (DWORD_PTR)hook;
                VirtualProtect(&t->u1.Function, sizeof(void *), old, &old);
                if (mod == g_exe) ListPatch((DWORD)(ULONG_PTR)&t->u1.Function, sizeof(void *));
                ++n;
            }
        }
    }
    return n;
}

static void PatchFocusImports(HMODULE mod)
{
    PatchIat(mod, "USER32.dll", "GetForegroundWindow", hkGetForegroundWindow, NULL);
    PatchIat(mod, "USER32.dll", "GetActiveWindow", hkGetActiveWindow, NULL);
    PatchIat(mod, "USER32.dll", "IsIconic", hkIsIconic, NULL);
    PatchIat(mod, "USER32.dll", "SetForegroundWindow", hkSetForegroundWindow, NULL);
    PatchIat(mod, "USER32.dll", "ClipCursor", hkClipCursor, NULL);
}

// Wrappers that may load after us: the OTS d3d8.dll, the GOG d3d8 wrapper (ori_d3d8.dll behind the OTS mod, d3d8.dll
// on a stock GOG install) and GOG's dshow.dll.
static const char *const kWrappers[] = { "d3d8.dll", "ori_d3d8.dll", "dshow.dll" };
static HMODULE g_patched[3];

// v1.3 FitWrapper: GOG's wrapper reads dxcfg.ini [general] at every CreateDevice and Reset (ReadConfig, through
// GetPrivateProfileStringW). display= takes only desktop, application or an exact adapter mode "WxH@Hz" (anything else
// silently means the desktop); "application" makes its swap chain the game's own size (scale 1, no filter), so any
// game size and shape is drawn 1:1 and the window only needs the same shape. presentation=windowed keeps it a window.
// (v1.2's screen-size answers to the wrapper are gone: they moved its window / popup decision.)
// v1.5 Borderless: display=desktop (back buffer = the monitor; the wrapper makes the window a popup over it),
// aspect=enabled and scaling=fit (the picture letterboxed undistorted, whatever the user's dxcfg.ini says).
static volatile LONG g_iniLogs, g_lastDisplayLog = -1;
static DWORD WINAPI hkGetPrivateProfileStringW(LPCWSTR app, LPCWSTR key, LPCWSTR def, LPWSTR out, DWORD n, LPCWSTR file)
{
    const WCHAR *v = NULL, *name = file ? wcsrchr(file, L'\\') : NULL;
    name = name ? name + 1 : file;
    if (app && key && out && n > 12 && name && !_wcsicmp(name, L"dxcfg.ini") && !_wcsicmp(app, L"general")) {
        const int bl = g_dm == DM_BORDERLESS;
        if (!_wcsicmp(key, L"display")) {
            v = bl ? L"desktop" : L"application";
            if (InterlockedExchange(&g_lastDisplayLog, bl) != bl) Log("wrapper: dxcfg.ini display read as %ls", v);
        } else if (!_wcsicmp(key, L"presentation")) {
            v = L"windowed";
            if (InterlockedIncrement(&g_iniLogs) <= 1) Log("wrapper: dxcfg.ini presentation read as %ls", v);
        } else if (bl && !_wcsicmp(key, L"aspect")) {
            v = L"enabled";
        } else if (bl && !_wcsicmp(key, L"scaling")) {
            v = L"fit";
        }  // v3.39 PR339: vsync and [enhancements] antialiasing / anisotropic pass through (dxcfg.exe sets them)
    }
    if (!v) return GetPrivateProfileStringW(app, key, def, out, n, file);
    wcscpy_s(out, n, v);
    return (DWORD)wcslen(v);
}

static void PatchWrappers(void)
{
    int i;
    for (i = 0; i < 3; ++i) {
        HMODULE m = GetModuleHandleA(kWrappers[i]);
        if (!m || m == g_patched[i]) continue;
        g_patched[i] = m;
        if (i < 2) {
            PatchIat(m, "USER32.dll", "ShowWindow", hkShowWindow, NULL);
            PatchIat(m, "USER32.dll", "SetWindowLongA", hkSetWindowLongA, NULL);
            PatchIat(m, "USER32.dll", "SetWindowLongW", hkSetWindowLongW, NULL);
            PatchIat(m, "USER32.dll", "SetWindowPlacement", hkSetWindowPlacement, NULL);
            if (g_fitWrapper)  // v1.3
                Log("wrapper: %d dxcfg.ini import(s) of %s hooked (display=application, presentation=windowed)",
                    PatchIat(m, "KERNEL32.dll", "GetPrivateProfileStringW", hkGetPrivateProfileStringW, NULL), kWrappers[i]);
        }
        PatchFocusImports(m);
        Log("window: hooked %s", kWrappers[i]);
    }
}

static void HookDirectInput(void)
{
    if (g_diHooked) return;
    if (PatchIat(g_exe, "DINPUT8.dll", "DirectInput8Create", hkDirectInput8Create, (void **)&oDirectInput8Create)) {
        g_diHooked = 1;
        Log("window: DirectInput8Create hooked (devices run non-exclusive)");
    }
}

static BOOL CALLBACK FindGameWindow(HWND hwnd, LPARAM lp)
{
    DWORD pid;
    char cls[64];
    GetWindowThreadProcessId(hwnd, &pid);
    GetClassNameA(hwnd, cls, sizeof cls);
    if (pid == GetCurrentProcessId() && IsWindowVisible(hwnd) && !GetParent(hwnd) && strcmp(cls, "IME")) {
        *(HWND *)lp = hwnd;
        return FALSE;
    }
    return TRUE;
}

// v3.41 VW341: Borderless, what covers the game's edges. The user's "narrow crack at the edges" (menus, memos, journals,
// cut-scenes: dark screens) is not in the game's picture: in the 4:3 shots the grey column lies in the letterbox that GOG's
// wrapper clears to black at every Present (ori_d3d8.dll 0x19005FAA viewport = the whole display, 0x19006410 Clear), and
// the Borderless client is the whole monitor (FillMonitor checks it). Windows composes other windows over a window that is
// not topmost (this mod keeps the game out of the topmost band, see HookProc): a neighbouring program's window edge or
// an auto-hidden taskbar's sliver then shows as a 1-2 px line along an edge. Once a second (the Watch thread), the windows
// above the game in the z-order are checked; a thin strip of one along an edge (VwEdgeSliver, at most 16 px; a taskbar
// up to 128 px) is logged with its window class (no titles), and again when that changes (24 lines a session). Hidden
// [Window] EdgeCoverLog=0: off.
typedef HRESULT(WINAPI *DwmAttr_t)(HWND, DWORD, void *, DWORD);
static void EdgeCoverCheck(void)
{
    static DWORD s_next;
    static char s_last[420];
    static volatile LONG s_q;
    static DwmAttr_t s_dwm;
    static int s_dwmTried;
    char sig[420];
    RECT gr;
    VwRect game;
    HWND h;
    int o = 0, n = 0, k;
    const DWORD now = GetTickCount();
    if (!g_edgeLog || !g_hwnd || (LONG)(now - s_next) < 0) return;
    s_next = now + 1000;
    if (g_dmCur != DM_BORDERLESS || IsIconic(g_hwnd) || !GetWindowRect(g_hwnd, &gr)) {
        s_last[0] = 0;
        return;
    }
    if (!s_dwmTried) {
        HMODULE d = LoadLibraryA("dwmapi.dll");
        s_dwmTried = 1;
        s_dwm = d ? (DwmAttr_t)GetProcAddress(d, "DwmGetWindowAttribute") : NULL;
    }
    game.l = gr.left; game.t = gr.top; game.r = gr.right; game.b = gr.bottom;
    sig[0] = 0;
    for (h = GetWindow(g_hwnd, GW_HWNDPREV), k = 0; h && k < 256; h = GetWindow(h, GW_HWNDPREV), ++k) {
        RECT wr, fb;
        VwRect win, cut, fr, c2;
        DWORD cloaked = 0;
        LONG ex;
        int e, frame = 0, thick;
        char cls[64];
        if (!IsWindowVisible(h) || IsIconic(h) || !GetWindowRect(h, &wr)) continue;
        if (s_dwm && SUCCEEDED(s_dwm(h, 14 /* DWMWA_CLOAKED */, &cloaked, sizeof cloaked)) && cloaked) continue;
        ex = GetWindowLongA(h, GWL_EXSTYLE);
        if ((ex & WS_EX_LAYERED) && (ex & WS_EX_TRANSPARENT)) continue;  // click-through overlays (they draw nothing there)
        win.l = wr.left; win.t = wr.top; win.r = wr.right; win.b = wr.bottom;
        VwEdgeSliver(win, game, 0, &cut);
        if (cut.r <= cut.l || cut.b <= cut.t) continue;  // not over the game at all
        if (!GetClassNameA(h, cls, sizeof cls)) strcpy_s(cls, sizeof cls, "?");
        // a taskbar over the game counts up to 128 px (shown or auto-hidden), any other window only as a thin strip
        thick = !strncmp(cls, "Shell_", 6) && strstr(cls, "TrayWnd") ? 128 : 16;
        if (!(e = VwEdgeSliver(win, game, thick, &cut))) continue;
        if (s_dwm && SUCCEEDED(s_dwm(h, 9 /* DWMWA_EXTENDED_FRAME_BOUNDS */, &fb, sizeof fb))) {
            fr.l = fb.left; fr.t = fb.top; fr.r = fb.right; fr.b = fb.bottom;
            frame = VwEdgeSliver(fr, game, thick, &c2) != 0;
        }
        if (o < (int)sizeof sig - 160)
            o += sprintf_s(sig + o, sizeof sig - o, "%s%s edge %ldx%ld at %ld,%ld (class %s, %s)", n ? "; " : "", VwEdgeName(e),
                           cut.r - cut.l, cut.b - cut.t, cut.l - game.l, cut.t - game.t, cls,
                           frame ? "its frame" : "its invisible border");
        ++n;
    }
    if (strcmp(sig, s_last)) {
        if (LogQuota(&s_q, 24)) {
            if (n)
                LogV("window: borderless edges: %d window(s) above the game cover its edges: %s; the game is %s", n, sig,
                     InFront() ? "in front" : "not in front (another program is active)");
            else
                LogV("window: borderless edges: nothing covers them now");
        }
        strcpy_s(s_last, sizeof s_last, sig);
    }
}

// Patches wrappers as they load, attaches if the window predates us, and releases the cursor if focus left by a path
// the window never saw.
static DWORD WINAPI Watch(LPVOID unused)
{
    int i, replaced = 0;
    (void)unused;
    for (i = 0;; ++i) {
        // v1.3: woken early when the game picked a new size (the window takes the picture's shape)
        if (g_reshapeEvt && WaitForSingleObject(g_reshapeEvt, i < 600 ? 100 : 250) == WAIT_OBJECT_0) ReshapeWindow();
        else if (!g_reshapeEvt) Sleep(i < 600 ? 100 : 250);
        PatchWrappers();
        if (!g_diHooked && i < 600) HookDirectInput();
        if (!g_hwnd && i < 300) {
            HWND found = NULL;
            EnumWindows(FindGameWindow, (LPARAM)&found);
            if (found && !g_hwnd) Attach(found);
        }
        // v1.1 safety net: for 15 s after attaching, a window that is not the size we placed it at (sized before we
        // attached, or by a path our window procedure never saw) is placed again, unless the user has sized it
        if (g_hwnd && g_attachTick && !g_userSized && replaced < 5 && g_dmCur == DM_WINDOWED && GetTickCount() - g_attachTick < 15000 &&
            !IsZoomed(g_hwnd) && !IsIconic(g_hwnd)) {
            RECT r;
            if (GetClientRect(g_hwnd, &r) && (r.right != g_cliW || r.bottom != g_cliH)) {
                Log("window: client %ldx%ld after attach, placing it again at %dx%d", r.right, r.bottom, g_cliW, g_cliH);
                PlaceWindow(g_hwnd);
                ++replaced;
            }
        }
        if (g_confined && !InFront()) {
            ClipCursor(NULL);
            g_confined = 0;
        }
        EdgeCoverCheck();  // v3.41 VW341
    }
}

static void WindowInit(void)
{
    g_style = g_resizable ? WS_OVERLAPPEDWINDOW : (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
    g_winStyle = g_style;
    PatchIat(g_exe, "USER32.dll", "CreateWindowExA", hkCreateWindowExA, (void **)&oCreateWindowExA);
    PatchIat(g_exe, "USER32.dll", "ShowWindow", hkShowWindow, (void **)&oShowWindow);
    PatchIat(g_exe, "USER32.dll", "SetWindowLongA", hkSetWindowLongA, (void **)&oSetWindowLongA);
    PatchIat(g_exe, "USER32.dll", "SetCursorPos", hkSetCursorPos, (void **)&oSetCursorPos);
    PatchFocusImports(g_exe);
    if (!oShowWindow) oShowWindow = ShowWindow;
    if (!oSetCursorPos) oSetCursorPos = SetCursorPos;
    if (!oSetWindowLongA) oSetWindowLongA = SetWindowLongA;
    if (!oCreateWindowExA) oCreateWindowExA = CreateWindowExA;
    HookDirectInput();  // the watcher retries if DINPUT8.dll was not bound yet
    PatchWrappers();
    g_reshapeEvt = CreateEventA(NULL, FALSE, FALSE, NULL);
    CloseHandle(CreateThread(NULL, 0, Watch, NULL, 0, NULL));
    Log("window: hooks installed (client %dx%d; DirectInput8Create %s)", g_width, g_height, g_diHooked ? "hooked" : "not bound yet");
}

static void ControllerFixInit(void)
{
    HMODULE di = GetModuleHandleA("dinput8.dll");
    if (!di) {
        Log("controller fix: dinput8.dll not loaded, nothing to do");
        return;
    }
    if ((g_tlsRead = TlsAlloc()) == TLS_OUT_OF_INDEXES) {
        Log("controller fix: TlsAlloc failed");
        return;
    }
    Log("controller fix: dinput8.dll ReadFile %d, GetOverlappedResult %d, CloseHandle %d import(s) wrapped",
        PatchIat(di, "KERNEL32.dll", "ReadFile", hkWrapReadFile, NULL),
        PatchIat(di, "KERNEL32.dll", "GetOverlappedResult", hkWrapGetOverlappedResult, NULL),
        PatchIat(di, "KERNEL32.dll", "CloseHandle", hkWrapCloseHandle, NULL));
}

// ================================================================ other window code in the process

// Reads a module's export table (works on a mapped dll whose DllMain has not run yet).
static int HasExport(HMODULE mod, const char *name)
{
    BYTE *base = (BYTE *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY d = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    IMAGE_EXPORT_DIRECTORY *ex;
    DWORD i, *names;
    if (!d.VirtualAddress) return 0;
    ex = (IMAGE_EXPORT_DIRECTORY *)(base + d.VirtualAddress);
    names = (DWORD *)(base + ex->AddressOfNames);
    for (i = 0; i < ex->NumberOfNames; ++i)
        if (!strcmp((const char *)(base + names[i]), name)) return 1;
    return 0;
}

// FILEVERSION of a loaded module's version resource (major << 16 | minor), 0 = none.
static DWORD ModuleVersion(HMODULE mod)
{
    HRSRC r = FindResourceA(mod, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);
    HGLOBAL g = r ? LoadResource(mod, r) : NULL;
    const BYTE *p = g ? (const BYTE *)LockResource(g) : NULL;
    DWORD n = r ? SizeofResource(mod, r) : 0, i;
    for (i = 0; p && i + 52 <= n; i += 4)
        if (*(const DWORD *)(p + i) == 0xFEEF04BD) return *(const DWORD *)(p + i + 8);
    return 0;
}

// Another window implementation already in the process: the Playable Eileen plugin up to v0.81 (window.c inside
// dsound.dll; v0.82 dropped it) or the laptop rig's SH4Windowed.asi. Two of them would subclass the same window and
// hook the same imports, so this mod then stays a plain winmm forwarder.
static const char *OtherWindowMod(void)
{
    HMODULE ds = GetModuleHandleA("dsound.dll");
    if (ds && HasExport(ds, "SH4PE_Patches")) {
        DWORD v = ModuleVersion(ds);
        if (v < 0x00000052) return "the Playable Eileen plugin up to v0.81 (dsound.dll) has its own windowed mode; update it to v0.82 or later";
    }
    if (GetModuleHandleA("SH4Windowed.asi")) return "SH4Windowed.asi is loaded (scripts folder); remove it to use this mod";
    return NULL;
}

// ================================================================ loading

static HANDLE g_onceMutex;

#include "texpack.inc"  // [Textures] Pack (prototype, 2026-11-09)
#include "font.inc"  // v3.43 FN343: [Textures] MenuFont, the HD glyph page
static void QsFrame(void);  // v3.6.0, quicksave.inc (called from the HUD hook in items.inc)
static void FxSceneFrame(void);  // v3.16.0, screenfx.inc (the scene filter, called from the HUD hook in items.inc)
static void DpTick(void);        // v3.27.0, deathpresence.inc (Death Presence: the 0x4EF982 dispatcher runs it)
static void DropTick(void);      // SUPPLY DROPS, drops.inc (the 0x4EF982 dispatcher runs it)
static void DropWorldReset(void);  // v3.30 AS330: drops.inc (a new roll seed: enemies.inc's world-reset hook calls it)
static void DpDrawFrame(void);   // v3.27.0, deathpresence_draw.inc (the fog: the HUD hook in items.inc runs it)
static void AiTick(void);        // v3.29 AFTERIMAGE, afterimage.inc (the stun: the 0x4EF982 dispatcher runs it)
static void AiDrawFrame(void);   // v3.29 AFTERIMAGE, afterimage.inc (the trail: the HUD hook in items.inc runs it)
static void EchoTick(void);      // v3.30 ECHO, echo.inc (the Echo double: the 0x4EF982 dispatcher runs it)
static void EchoDrawFrame(void); // v3.30 ECHO, echo.inc (its draw: the HUD hook in items.inc runs it)
static void EnmTick(void);        // v3.31 ENEMIES, enemies.inc (Enemy Copies / Enemies Follow: the 0x4EF982 dispatcher runs it)
static void GtTick(void);         // v3.43 GT343, teleport.inc (Ghost Teleport: the 0x4EF982 dispatcher runs it)
static void PkTick(void);         // v3.32 PM332, pickup.inc (a dropped item under a downed ghost: the 0x4EF982 dispatcher runs it)
static void PpTick(void);         // v3.35 PK335, pickpool.inc (the pickup pool's per-room line: the 0x4EF982 dispatcher runs it)
static void PoolsTick(void);      // v3.36 TP336, pools.inc (the task / IK pools' live use: the 0x4EF982 dispatcher runs it)
static void HpTick(void);         // v3.36 HE336, hitpool.inc (the hit-shape pool's free count and per-room line: the dispatcher)
static void EvTick(void);         // v3.36 HE336, recevict.inc (the enemy records' per-room line: the dispatcher)
static int HpShapeInPool(const BYTE *hs);  // v3.36 HE336, hitpool.inc (echo.inc EchoHurtOk: the game's 100 shapes or our block)
static void LhFrameBegin(void);   // v3.33 LOG333 (W7), loghealth.inc (the health log: the HUD hook in items.inc runs both)
static void LhFrameEnd(void);
static void RfWarmFrame(void);   // v3.35 FX335: echo.inc (every effect made ready at a room's first frame: the HUD hook runs it)
#include "budget.inc"   // v3.35 FX335: the effect budget and the frame ledger (before items.inc: the scene pass's parts are timed)
#include "items.inc"    // v3.3 [Items] Sparkle
#include "quicksave.inc"  // v3.6.0 Quick Save / Quick Load
#include "extramenu.inc"  // v3.7.0 the pause Extra Menu
#include "savebanks.inc"  // v3.8.0 Save Banks
#include "eileenoutfit.inc"  // v3.9.0 Eileen outfit engine
#include "buttoncfg.inc"  // v3.10.0 Button Config pad names
#include "subtitles.inc"  // v3.13.0 subtitle style
#include "screenfx.inc"   // v3.15.0 Screen Effects (noise strength, filters); v3.16.0 scene pass, Noise Type, Thermal
#include "shadows.inc"  // v3.40 SH340: Shadow Edge (softer shadows) and the Shadows page's preview (after screenfx.inc: its helpers)
#include "dof.inc"  // v3.43 DF343: Depth of Field (depth slices) and its page (after shadows.inc: chains its reset hook)
#include "depthfix.inc"  // v3.31 DEPTH331: the player's depth restored before Death Presence / Echo draw
#include "deathpresence.inc"  // v3.27.0 Death Presence (special-mod round 2)
#include "deathpresence_draw.inc"  // v3.27.0 its fog
#include "afterimage.inc"  // v3.29 AFTERIMAGE: the after-image stun "Volt Trail" (special-mod round 3)
#include "echo.inc"  // v3.30 ECHO: the Echo double (special-mod round R4)
#include "enemies.inc"  // v3.31 ENEMIES Enemy Copies, Enemies Follow (special-mod round 5)
#include "rzlooks.inc"  // v3.49 RZ349L: Randomizer > Eileen Outfit / Ghosts (after eileenoutfit.inc and enemies.inc: their engine and spot test)
#include "teleport.inc"  // v3.43 GT343: Ghost Teleport (after deathpresence.inc / echo.inc / enemies.inc: their helpers)
#include "hudbars.inc"  // HUD BARS: Enemy Health Bars, Curse Meter (p2_hud candidate)
#include "pickpool.inc"  // v3.35 PK335: the dynamic pickup pool, 128 records in this dll + the full-pool guard (before drops.inc)
#include "pools.inc"  // v3.36 TP336: the task pool 300 -> 400 in place, the IK pool 160 -> 512, their per-room use
#include "hitpool.inc"   // v3.36 HE336: the hit-shape pool, 100 -> 356 (a block in this dll) + the stomp guard
#include "recevict.inc"  // v3.36 HE336: a full enemy-record table evicts copies' records first (after enemies.inc)
#include "rzmon.inc"  // v3.49 RZ349: the Randomizer's Monsters (after enemies.inc / hitpool.inc: their loader, spot test, records, pool size; randomizer.inc owns the row's setting)
#include "drops.inc"  // SUPPLY DROPS (Option 5 > Enemies)
#include "pickup.inc"  // v3.32 PM332: a dropped item under a downed ghost (the pickup wins over a stomp)
#include "loghealth.inc"  // v3.33 LOG333 (W7): the gameplay health log (session header, room lines, framework anomalies); last
#include "renderer_sim.h"  // v3.39 PR339: MD5, Graphics Plus's manifest, the Renderer row's state (test\rend_test.c)
#include "renderer.inc"  // v3.39 PR339: the Renderer row's file side (for Graphics Plus's review)
#include "presentation.inc"  // v3.39 PR339: Option 4's Renderer and Opening rows, the opening skip (before perfhud.inc)
#include "perfhud.inc"  // v3.37 CP337: the performance overlay and the backend line (after loghealth.inc: reads its ledger total)

// ---------------------------------------------------------------- v3.14.0: vibration strength (install, test key)
// Game thread (CreateDevice). GOG's wrapper writes ONE motor value to every pad it maps (RE 2.2), so the cap is the
// lowest any connected pad needs: 100 when an Xbox pad is driven over HID (Microsoft VID 045E, listed in goginput.ini,
// not an XInput "IG_" device: that path reads 0..100 [RE, inferred]), else 255. Review VIB-001: its own Raw Input scan,
// not Button Config's family guess (mixed pads -> Generic, PadNames override).
static LONG VibCapNow(void)
{
    UINT n = 0, i;
    RAWINPUTDEVICELIST *list;
    LONG cap = 255;
    if (GetRawInputDeviceList(NULL, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || !n || n > 256) return 255;
    list = (RAWINPUTDEVICELIST *)HeapAlloc(GetProcessHeap(), 0, n * sizeof *list);
    if (!list) return 255;
    n = GetRawInputDeviceList(list, &n, sizeof(RAWINPUTDEVICELIST));
    for (i = 0; i < n && n != (UINT)-1; ++i) {
        RID_DEVICE_INFO di;
        UINT sz = sizeof di, nsz;
        char name[256];
        if (list[i].dwType != RIM_TYPEHID) continue;
        memset(&di, 0, sizeof di);
        di.cbSize = sizeof di;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICEINFO, &di, &sz) == (UINT)-1) continue;
        if (di.hid.usUsagePage != 1 || (di.hid.usUsage != 4 && di.hid.usUsage != 5) || di.hid.dwVendorId != 0x045E) continue;
        name[0] = 0;
        nsz = sizeof name;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name, &nsz) == (UINT)-1) name[0] = 0;
        name[sizeof name - 1] = 0;
        if (strstr(name, "IG_") || strstr(name, "ig_")) continue;  // XInput: not this path
        if (PadListed(di.hid.dwVendorId, di.hid.dwProductId)) cap = 100;
    }
    HeapFree(GetProcessHeap(), 0, list);
    return cap;
}
static void __cdecl VibTestTick(void)  // [Input] VibrationTest=1: F10 in gameplay plays the game's rumble 1 (0.4 s)
{
    static int s_down;
    const int down = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (down && !s_down && *(volatile int *)0x1236D4C == 6) {
        ((void(__cdecl *)(int))0x4F7FE0)(1);
        Log("vibration: test rumble 1 (F10) at %lu ms, level %s, Vibration %s", GetTickCount(), kVibName[g_vibLevel], CFG[0x0B] ? "On" : "Off");
    }
    s_down = down;
}
static DWORD WINAPI VibHookRetry(LPVOID unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 600 && !g_diHooked; ++i) {
        Sleep(100);
        HookDirectInput();
    }
    if (!g_diHooked) Log("vibration: DirectInput8Create was never bound: the strength stays the game's own");
    return 0;
}
static void VibInstall(void)
{
    char buf[32];
    int v;
    IniStr("Input", "Vibration", buf, sizeof buf);
    for (v = 3; v > 1 && _stricmp(buf, kVibName[v]); --v) {}
    g_vibLevel = v;  // unknown text = Low (the game's own)
    g_vibTest = GetPrivateProfileIntA("Input", "VibrationTest", 0, g_iniPath) != 0;  // hidden
    if (g_vibTest) g_vibTick = VibTestTick;
    g_vibOn = 1;
    if (!g_enable) {  // Enable=1: WindowInit hooks it (and its watcher retries)
        HookDirectInput();
        if (!g_diHooked) {  // review VIB-005: retry until DINPUT8.dll is bound (as the window watcher does), 60 s at most
            HANDLE th = CreateThread(NULL, 0, VibHookRetry, NULL, 0, NULL);
            if (th) CloseHandle(th);
        }
    }
    Log("vibration: strength %s; GOG's force-feedback command (device slot 22) hooked as devices are made%s", kVibName[v],
        g_vibTest ? "; test rumble on F10" : "");
}

static void Init(void)
{
    char path[MAX_PATH], exe[MAX_PATH], *slash, *name, mx[64];
    const char *other;
    GetModuleFileNameA(g_self, g_dir, MAX_PATH);
    slash = strrchr(g_dir, '\\');
    if (slash) slash[1] = 0;
    GetModuleFileNameA(NULL, exe, MAX_PATH);
    name = strrchr(exe, '\\');
    name = name ? name + 1 : exe;
    if (_stricmp(name, "SILENT HILL 4.exe")) return;  // any other program in the folder: forwarding only, no files
    sprintf_s(mx, sizeof mx, "Local\\SH4WindowMod.%lu", GetCurrentProcessId());  // one instance per process
    g_onceMutex = CreateMutexA(NULL, FALSE, mx);
    if (!g_onceMutex || GetLastError() == ERROR_ALREADY_EXISTS) return;
    {   // v2.1.1: SH4Refined.* names (the old SH4WindowMod.* files are moved once); keep the previous session's log
        char names[1024];
        static char rot[2048], rotAnom[512];  // v3.40 LG340: LogRotate's notes until the log is open
        SettingsPath(g_dir, g_iniPath, sizeof g_iniPath);  // v3.40 LG340: [Log] Keep before the logs move (read again below)
        LogRotate(g_dir, LrKeep(GetPrivateProfileIntA("Log", "Keep", LR_KEEP_DEFAULT, g_iniPath)), rot, sizeof rot, rotAnom,
                  sizeof rotAnom);  // v3.40 LG340: copies SH4Refined.prev.log / .log into logs\ (the rename below as before)
        RenameOldFiles(g_dir, names, sizeof names);
        SettingsPath(g_dir, g_iniPath, sizeof g_iniPath);
        sprintf_s(path, sizeof path, "%s" SH4R_LOG, g_dir);
        g_log = _fsopen(path, "w", _SH_DENYNO);  // readable while the game runs
        Log("SH4 Refined v%s loading (winmm.dll proxy, game base %p)", SH4WM_VERSION, GetModuleHandleA(NULL));
        g_logLevel = GetPrivateProfileIntA("Log", "Level", 2, g_iniPath);  // v3.33 LOG333
        if (g_logLevel < 1) g_logLevel = 1;
        if (g_logLevel > 3) g_logLevel = 3;
        Log("log: level %d (%s; [Log] Level 1 normal, 2 verbose, 3 trace); grep \"ANOMALY:\" for states a feature should "
            "never reach", g_logLevel, g_logLevel == 1 ? "normal" : g_logLevel == 2 ? "verbose" : "trace");
        if (names[0]) Log("files: %s", names);
        LogRotateReport(rot, rotAnom);  // v3.40 LG340: "log: started <date>" (the next start names this session by it), what logs\ got
    }
    g_exe = GetModuleHandleA(NULL);
    if ((other = OtherWindowMod()) != NULL) {
        Log("not active: %s. winmm calls are still forwarded.", other);
        return;
    }
    ReadIni();
    Log("settings: Enable %d, %dx%d, Resizable %d, KeepActiveOnAltTab %d, CaptureMouse %d, SnapToStandard %d, NoMinimize %d, "
        "AspectMenu %d, Menus43 %d, MenuPillars %d, Menus43Log %d, FitWrapper %d, ControllerCrashFix %d", g_enable, g_width, g_height,
        g_resizable, g_keepActive, g_capture, g_snap, g_noMinimize, g_aspectMenu, g_menus43Ini, g_menuPillars, g_menus43Log,
        g_fitWrapper, g_hidFix);
    Log("settings: Monitor \"%s\", MonitorName \"%s\", MonitorPath \"%s\"", g_monIni, g_monNameIni, g_monPathIni);
    MonInit();  // v1.7
    {   // hooks point into this dll: keep it loaded for the life of the process
        HMODULE pin;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)Init, &pin);
    }
    if (g_exe == (HMODULE)0x400000) ApplyFrameRate();  // v1.9: before the exe's init reads the rate
    if (g_exe == (HMODULE)0x400000) SubtitlesInstall();  // v3.13.0: before the Options tables (ApplyAspectMenu)
    if (g_exe == (HMODULE)0x400000) VibInstall();  // v3.14.0: before the Options tables too
    if (g_exe == (HMODULE)0x400000) ScreenFxInstall();  // v3.15.0: before the Options tables too
    if (g_exe == (HMODULE)0x400000) ShadowsInstall();  // v3.40 SH340: after ScreenFxInstall (its backdrop hook), before the Options tables
    if (g_exe == (HMODULE)0x400000) DofInstall();  // v3.43 DF343: after ShadowsInstall (chains its reset hook 0x402E0D and screenfx's 0x526A53), before the Options tables
    if (g_exe == (HMODULE)0x400000) GameplayInstall();  // v3.25.0: before the Options tables too
    if (g_exe == (HMODULE)0x400000) CameraInstall();  // v3.33 CAM333: before the Options tables too (decides who owns the view)
    if (g_exe == (HMODULE)0x400000) RendBootRead();  // v3.39 PR339: before the Options tables (the Renderer row's first state); REV339-004: before PerfInstall (the variant name)
    if (g_exe == (HMODULE)0x400000) PerfInstall();  // v3.37 CP337: before the Options tables too (the backend line's texts)
    if (g_exe == (HMODULE)0x400000) OpeningInstall();  // v3.39 PR339: before the exe's first tick (mode 1 runs the boot screens)
    if (g_noMinimize || g_aspectMenu) {
        // ApplyAspectMenu installs EVERY Refined Options row and page (Option 1's Aspect Ratio and Menu Font rows, the
        // Screen Effects, Shadows and Gameplay pages, ...): [Window] AspectMenu=0 leaves the stock Options and every setting
        // in the ini (the menu font too: FontInstall below reads [Textures] MenuFont). Codex inspection 6, FN344-ASPECT-1.
        if (g_exe == (HMODULE)0x400000) {
            ApplyExeFixes();
            if (g_aspectMenu) ApplyAspectMenu();
            else Log("options: [Window] AspectMenu=0: the stock Options pages; every Refined setting (the menu font and Omni-Directional Movement too) from the ini");  // v3.47 CT347: + Stick Movement  // v3.48 OM348: the name
        }
        else Log("unexpected image base %p: exe fixes not applied", g_exe);
    }
    if (g_exe == (HMODULE)0x400000) ApplySaveTitles();  // v3.5.2
    if (g_exe == (HMODULE)0x400000) ApplyD3DMonitor();  // v1.7
    TexPackReadIni();  // [Textures] (off by default)
    if (g_exe == (HMODULE)0x400000) TexPackInstall();
    if (g_exe == (HMODULE)0x400000) FontInstall();  // v3.43 FN343: [Textures] MenuFont (Stock = nothing installed)
    if (g_exe == (HMODULE)0x400000) SparkleInstall();  // v3.3
    if (g_exe == (HMODULE)0x400000) ScreenFxReport();  // v3.16.0: the scene filter needs SparkleInstall's HUD hook
    // v3.18.0 (review SEFX3-001): the 0x4EF982 dispatcher also without Quick Save / the Extra Menu (the pause filter's
    // overlay snapshot); with both off it runs only the stock call and the snapshot
    if (g_exe == (HMODULE)0x400000 && QuickSaveInstall(1) && g_emEnabled) ExtraInstall();  // v3.6.0 / v3.7.0
    if (g_exe == (HMODULE)0x400000) BldFallbackStart();  // v3.25.0 (review REV-009)
    if (g_exe == (HMODULE)0x400000) DeathPresenceInstall();  // v3.27.0: needs the 0x4EF982 dispatcher (QuickSaveInstall)
    if (g_exe == (HMODULE)0x400000) AfterImageInstall();  // v3.29 AFTERIMAGE: no exe patch; needs the dispatcher and the screen-effects hooks
    if (g_exe == (HMODULE)0x400000) EchoInstall();  // v3.30 ECHO: no exe patch; needs the same dispatcher, scene pass and hooks
    if (g_exe == (HMODULE)0x400000) DepthFixInstall();  // v3.31 DEPTH331: no exe patch; needs the draw-call override and the model hook
    if (g_exe == (HMODULE)0x400000) EnemiesInstall();  // v3.31 ENEMIES: after GameplayInstall and QuickSaveInstall
    if (g_exe == (HMODULE)0x400000) PickPoolInstall();  // v3.35 PK335: before the exe's init (the pool is still zero)
    if (g_exe == (HMODULE)0x400000) PoolsInstall();  // v3.36 TP336: before the exe's init; after EnemiesInstall (its 0x4F0A55 check)
    if (g_exe == (HMODULE)0x400000) HitPoolInstall();  // v3.36 HE336: before the exe's init (HitInit builds the pool)
    if (g_exe == (HMODULE)0x400000) RecEvictInstall();  // v3.36 HE336: after EnemiesInstall (its purge keeps copies' state)
    if (g_exe == (HMODULE)0x400000) DropsInstall();  // SUPPLY DROPS: needs the 0x4EF982 dispatcher too
    if (g_exe == (HMODULE)0x400000) RegenInstall();  // v3.47 RG347: Refined's own Regeneration (no exe patch; needs the dispatcher)
    if (g_exe == (HMODULE)0x400000) PickupInstall();  // v3.32 PM332: needs the 0x4EF982 dispatcher too
    if (g_exe == (HMODULE)0x400000) SaveBanksInstall();  // v3.8.0: always (review SB-001); the hooks follow g_sbOn live
    if (g_exe == (HMODULE)0x400000 && g_eoEnabled) EileenOutfitInstall();  // v3.9.0
    if (g_exe == (HMODULE)0x400000) ButtonConfigInstall();  // v3.10.0
    if (g_hidFix) ControllerFixInit();
    if (g_enable) WindowInit();
    else Log("window: disabled ([Window] Enable=0): the game keeps its own window");
    Log("%lu range(s) listed in SH4WM_Patches", SH4WM_Patches.count);
    LhSessionHeader();  // v3.33 LOG333 (W7)
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        Init();
    } else if (reason == DLL_PROCESS_DETACH) {
        LhSessionEnd(reserved ? "process exit" : "unloaded");  // v3.33 LOG333 (W7): only when the log's lock is free
        if (g_log && !reserved) {  // FreeLibrary only: at process exit other threads died, maybe holding the FILE lock
            Log("unloading");
            fclose(g_log);
            g_log = NULL;
        }
    }
    return TRUE;
}

