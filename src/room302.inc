// ================================================================ v3.50 R302: Room 302's look and walk, Henry and Eileen alike
// The user (USER_FEEDBACK_v349 D1): "I want to give Playable Eileen's Room 302 functionality over to Refined Mod. So that
// both Henry and Eileen can have the same settings." Plan, research and live checklist: staging/v350_feedback/w6_room302/
// PLAN.md; the hand-over Playable Eileen would need: R302_CONTRACT.md. Pure code: r302_sim.h (test\r302_sim_test.c).
//
// Two groups move (r302_sim.h R302F_*), the rest of Playable Eileen's Room 302 (her body, eye height, arm reach, side
// step, the body camera's feel, PadAnalog) stays hers:
//   LOOK: Look Down / Look Up limits and Look Feel. The stock limit tests of Room302Look 0x52DBD0 read Refined's floats
//         (0x52DCC6 `fld [0x5BD6D8]` and 0x52DD14 `fld [0x5C5BC0]`: the operands re-aimed, the constants are shared);
//         after Room302LookApply (0x52DF74 `call 0x52DDA0`, edi = the player) the deltas P+0x60 / P+0x64 are eased by
//         Look Feel and the pitch is clamped exactly on the view 0x10834D0 (a port of Playable Eileen v1.24.4's
//         FpLookAfter / FpClampView, which it ran for Eileen only). All three sites or none.
//   WALK: Walk Speed. No new site: Movement Speed's Room 302 operands (0x52D89C / 0x52D8E8, gameplay.inc) read
//         g_r302Walk = 10.5 x Movement Speed x Walk Speed in stage 1 (R3Tick sets it every tick; stage 0x0A keeps 1).
// The hand-over (r302_sim.h R3DecideOwner): Playable Eileen loaded without the export SH4PE_Room302Yield (v1.24.4 and
// older, the user's today) keeps both groups: nothing is hooked, the walk factor stays 1, the page's rows are grey with
// "Playable Eileen does this for now.". Without Playable Eileen, or with one that yields a group (and does not list its
// sites), the group is Refined's. A yield not yet decided at Init is decided again on the first game tick. The export
// SH4R_Room302 (winmm.def @200, DATA) tells Playable Eileen what Refined offers and took, and the look's last deltas.
// Rows: Options > Option 4 (Presentation) > Room 302 (a folder row above Exit) -> the hidden page LP_R302: Look Down,
// Look Up, Look Feel, Walk Speed, Back. SH4Refined.ini [Room302] LookDownLimit / LookUpLimit / LookFeel / WalkSpeed;
// a missing key is seeded once from SH4PlayableEileen.ini [Room302] (the user's values today), else 85 / 85 / 1 / 0.65.

#include "r302_sim.h"

#define MSG_R3 (MSG_V50 + 32)  // V50 + 32 .. 63 (BRIEF_COMMON: w6's)
enum { R3M_ROW, R3M_ROWD, R3M_ROWD_PE, R3M_TITLE, R3M_L0, R3M_D0 = R3M_L0 + R3K_N, R3M_DPE = R3M_D0 + R3K_N, R3M_DNA,
       R3M_GAME, R3M_V35, R3M_V45, R3M_F05, R3M_F1, R3M_F15, R3M_F2, R3M_W055, R3M_W06, R3M_W065, R3M_W07, R3M_W075,
       R3M_W08, R3M_W09, R3M_W11, R3M_W12, R3M_N };
typedef char R3TextsFit[R3M_N <= 32 && MSG_R3 + 32 <= MSG_END ? 1 : -1];
enum { R3G_LOOK, R3G_WALK, R3G_N };

// ---------------------------------------------------------------- state
R3Export SH4R_Room302 = { R3X_MAGIC, R3X_VERSION, sizeof(R3Export), R302F_ALL, 0, 0, 0, 0.0f, 0.0f, 0.0f, 50.0f, 25.0f, 0.0f, 1.0f };
static volatile LONG g_r3Own[R3G_N] = { R3_OWN_NONE, R3_OWN_NONE };
static int g_r3Decided;                     // R3Decide ran with a final answer (no group waiting)
static int g_r3Idx[R3K_N] = { 6, 6, 2, 3 }; // the rows' values (85, 85, 1, 0.65 until the ini is read)
static int g_r3Snap[R3K_N];
static float g_r3UpLim = R3_STOCK_UP, g_r3DownLim = R3_STOCK_DOWN;  // the operands of 0x52DCC6 / 0x52DD14 (ours: their reads)
static float g_r3Feel = 1.0f, g_r3Walk = 0.65f;
static volatile LONG g_r3LookActive;        // the rows differ from the game's own look (r302_sim.h R3LookActive)
static int g_r3LookHooked, g_r3Fault;
static R3Ease g_r3Ease;
static DWORD g_r3LastMs;
static int g_r3Room = -1;
static HMODULE g_r3Pe;
static const R3PeYield *g_r3Yield;
static const CamPeList *g_r3PeList;
static DWORD g_r3PeVer;
static char g_r3PeIni[MAX_PATH];
static BYTE g_txtR3[R3M_N][64];
static DWORD g_r3Ids[R3K_N][R3_WALK_N];
static DWORD g_rowsR302[(R3K_N + 1) * 7 + 1];
static int g_r3Row = -1;                    // Option 4's Room 302 row (-1 = not there)
static int g_r3Said[R3K_N];                 // an ini value off the rows was logged
static volatile LONG g_r3VisitLogs, g_r3RebuildLogs, g_r3FaultLogs;
static struct {
    int in;                                 // the last tick was in stage 1
    DWORD t0, tLog;
    DWORD look, eased, clamped, up, down, rebuilt, walk;
} g_r3V;
static float R3WalkK(void);

static const Site kR3LookSites[3] = {
    { 0x52DF74, { 0xE8, 0x27, 0xFE, 0xFF, 0xFF }, 5 },        // call 0x52DDA0 (Room302LookApply)
    { 0x52DCC6, { 0xD9, 0x05, 0xD8, 0xD6, 0x5B, 0x00 }, 6 },  // fld [0x5BD6D8] (25 deg up)
    { 0x52DD14, { 0xD9, 0x05, 0xC0, 0x5B, 0x5C, 0x00 }, 6 },  // fld [0x5C5BC0] (-50 deg down)
};
static const DWORD kR3WalkVa[2] = { 0x52D89A, 0x52D8E6 };  // fmul [0x5C5BA8] / [g_r302Walk] (gameplay.inc's Movement Speed)

static const BYTE *R3Text(int id)
{
    const int i = id - MSG_R3;
    return i >= 0 && i < R3M_N ? g_txtR3[i] : g_txtR3[R3M_ROW];
}

// ---------------------------------------------------------------- Playable Eileen and the sites
static void R3Probe(void)
{
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    g_r3Pe = NULL, g_r3Yield = NULL, g_r3PeList = NULL, g_r3PeVer = 0;
    if (!ds || !HasExport(ds, "SH4PE_Patches")) return;
    g_r3Pe = ds;
    g_r3PeVer = ModuleVersion(ds);
    g_r3PeList = (const CamPeList *)GetProcAddress(ds, "SH4PE_Patches");
    if (g_r3PeList && g_r3PeList->magic != 0x45504853) g_r3PeList = NULL;
    if (HasExport(ds, "SH4PE_Room302Yield")) {
        const R3PeYield *y = (const R3PeYield *)GetProcAddress(ds, "SH4PE_Room302Yield");
        __try {
            if (y && y->magic == R3Y_MAGIC && y->version >= 1) g_r3Yield = y;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            g_r3Yield = NULL;
        }
        if (!g_r3Yield) LogAnomaly("room 302", "Playable Eileen exports SH4PE_Room302Yield but not in the contract's form: its Room 302 stays its own");
    }
}
static int R3PeLists(DWORD va, DWORD len)
{
    DWORD i, n;
    if (!g_r3PeList) return 0;
    n = g_r3PeList->count > 512 ? 512 : g_r3PeList->count;
    for (i = 0; i < n; ++i)
        if (g_r3PeList->item[i].va < va + len && va < g_r3PeList->item[i].va + g_r3PeList->item[i].len) return 1;
    return 0;
}
static void R3LookStub(void);
static int R3LookSiteState(void)
{
    const BYTE *c = (const BYTE *)(ULONG_PTR)0x52DF74;
    const DWORD up = (DWORD)(ULONG_PTR)&g_r3UpLim, down = (DWORD)(ULONG_PTR)&g_r3DownLim;
    int i, stock = 1;
    for (i = 0; i < 3; ++i)  // (a plain compare: Playable Eileen's hooks there are expected, no failed byte check of the start)
        stock &= !memcmp((const void *)(ULONG_PTR)kR3LookSites[i].va, kR3LookSites[i].stock, kR3LookSites[i].len);
    if (stock) return R3S_STOCK;
    if (c[0] == 0xE8 && 0x52DF79 + *(const DWORD *)(c + 1) == (DWORD)(ULONG_PTR)R3LookStub && *(const DWORD *)0x52DCC8 == up &&
        *(const DWORD *)0x52DD16 == down)
        return R3S_OURS;
    return R3S_OTHER;
}
static int R3WalkSiteState(void)
{
    const DWORD w = (DWORD)(ULONG_PTR)&g_r302Walk;
    return g_moveOn && *(const DWORD *)(kR3WalkVa[0] + 2) == w && *(const DWORD *)(kR3WalkVa[1] + 2) == w ? R3S_OURS : R3S_OTHER;
}
static const char *R3PeText(char *b, size_t n)
{
    if (!g_r3Pe) return "not loaded";
    if (!g_r3Yield)
        sprintf_s(b, n, "v%lu.%lu loaded without SH4PE_Room302Yield (older than the hand-over): its Room 302 look and walk "
                  "stay its own (Eileen only); Refined's Room 302 rows are greyed, nothing hooked", g_r3PeVer >> 16, g_r3PeVer & 0xFFFF);
    else if (!g_r3Yield->decided)
        sprintf_s(b, n, "v%lu.%lu loaded, SH4PE_Room302Yield not decided yet", g_r3PeVer >> 16, g_r3PeVer & 0xFFFF);
    else
        sprintf_s(b, n, "v%lu.%lu yields%s%s%s (SH4PE_Room302Yield)", g_r3PeVer >> 16, g_r3PeVer & 0xFFFF,
                  g_r3Yield->yield & R302F_LOOK ? " look" : "", (g_r3Yield->yield & R302F_ALL) == R302F_ALL ? "," : "",
                  g_r3Yield->yield & R302F_WALK ? " walk" : (g_r3Yield->yield & R302F_LOOK ? "" : " nothing"));
    return b;
}

// ---------------------------------------------------------------- the values
static void R3Apply(void)  // after a value or owner change (game thread / the menu)
{
    const int look = g_r3Own[R3G_LOOK] == R3_OWN_REFINED, walk = g_r3Own[R3G_WALK] == R3_OWN_REFINED;
    g_r3UpLim = R3UpLimit(g_r3Idx[R3K_UP]);
    g_r3DownLim = R3DownLimit(g_r3Idx[R3K_DOWN]);
    g_r3Feel = kR3Feel[g_r3Idx[R3K_FEEL]];
    g_r3Walk = kR3Walk[g_r3Idx[R3K_WALK]];
    InterlockedExchange(&g_r3LookActive, R3LookActive(g_r3Idx[R3K_DOWN], g_r3Idx[R3K_UP], g_r3Idx[R3K_FEEL]));
    SH4R_Room302.lookDown = look ? (float)kR3Down[g_r3Idx[R3K_DOWN]] : 50.0f;
    SH4R_Room302.lookUp = look ? (float)kR3Up[g_r3Idx[R3K_UP]] : 25.0f;
    SH4R_Room302.lookFeel = look ? g_r3Feel : 0.0f;
    SH4R_Room302.walkSpeed = walk ? g_r3Walk : 1.0f;
    g_r302Walk = 10.5f * g_moveK * R3WalkK();
}
// [Room302] key k: a missing one is seeded once (from Playable Eileen's [Room302] when its ini has one)
static void R3ReadKey(int k, int atStart)
{
    static const char *const kPeKey[R3K_N] = { "LookDownLimit", "LookUpLimit", "Feel", "WalkSpeed" };
    char buf[64], pe[64];
    float v;
    int exact, idx;
    if (g_iniFallback) {
        strcpy_s(buf, sizeof buf, kR3Def[k]);
    } else if (GetPrivateProfileStringA("Room302", kR3Key[k], "\x01", buf, sizeof buf, g_iniPath) == 1 && buf[0] == 1) {
        int peHas = 0, body = 1, from;
        char sec[8];
        if (!atStart) return;  // (only the start seeds; a key deleted later reads as before)
        peHas = GetFileAttributesA(g_r3PeIni) != INVALID_FILE_ATTRIBUTES &&
                GetPrivateProfileSectionA("Room302", sec, sizeof sec, g_r3PeIni) > 0;
        pe[0] = 0;
        if (peHas) {
            body = GetPrivateProfileIntA("Room302", "FirstPersonBody", 1, g_r3PeIni) != 0;
            GetPrivateProfileStringA("Room302", kPeKey[k], "", pe, sizeof pe, g_r3PeIni);
        }
        from = R3Seed(k, peHas, body, pe, buf, sizeof buf);
        WritePrivateProfileStringA("Room302", kR3Key[k], buf, g_iniPath);
        Log("settings: [Room302] %s was missing, added as %s%s", kR3Key[k], buf,
            from ? (body ? " (from SH4PlayableEileen.ini [Room302])" : " (the game's: SH4PlayableEileen.ini has FirstPersonBody=0, "
                    "so it applied none of it)") : "");
    }
    if (!R3ParseNum(buf, &v)) {
        if (!g_r3Said[k]++) Log("room 302: [Room302] %s=%s is not a number: %s used", kR3Key[k], buf, kR3Def[k]);
        R3ParseNum(kR3Def[k], &v);
    }
    idx = R3IndexOf(k, v, &exact);
    if (!exact && !g_r3Said[k]++) {
        char t[16];
        R3IniText(k, idx, t, sizeof t);
        Log("room 302: [Room302] %s=%s is not one of the row's values: %s used", kR3Key[k], buf, t);
    }
    g_r3Idx[k] = idx;
}
static void R3ReadIni(int atStart)
{
    int k;
    for (k = 0; k < R3K_N; ++k) R3ReadKey(k, atStart);
    R3Apply();
}
static void R3Set(int k, int idx, const char *why)
{
    char was[16], now[16];
    if (idx < 0 || idx >= kR3Count[k] || idx == g_r3Idx[k]) return;
    R3IniText(k, g_r3Idx[k], was, sizeof was);
    R3IniText(k, idx, now, sizeof now);
    g_r3Idx[k] = idx;
    if (!g_iniFallback && !WritePrivateProfileStringA("Room302", kR3Key[k], now, g_iniPath))
        Log("room 302: [Room302] %s could not be written (error %lu): %s for this session only", kR3Key[k], GetLastError(), now);
    R3Apply();
    Log("room 302: [Room302] %s %s -> %s (%s)", kR3Key[k], was, now, why);
}

// ---------------------------------------------------------------- the look (0x52DF74, game thread, Room 302's player task)
static void R3VisitFlush(const char *why);
static void __cdecl R3LookAfter(BYTE *P)
{
    __try {
        float *const m = (float *)0x10834D0;
        float *pd, *yd, *by, yaw, pitch;
        R3StepOut o;
        DWORD now;
        int room;
        if (g_r3Fault || !P || *(volatile DWORD *)0xFD5A90 != 1 || *(const int *)(P + 0x214) >= 100) return;
        pd = (float *)(P + 0x60), yd = (float *)(P + 0x64), by = (float *)(P + 0x84);
        now = GetTickCount();
        room = *(volatile int *)0xFD5A94;
        if (now - g_r3LastMs > 250 || room != g_r3Room) g_r3Ease.yawRate = g_r3Ease.pitchRate = 0.0f;  // no easing carried over a gap
        g_r3LastMs = now, g_r3Room = room;
        if (!g_r3LookActive) {  // the game's own look: published, untouched
            SH4R_Room302.yawRate = *yd, SH4R_Room302.pitchRate = *pd;
            if (R3ViewAngles(m, &yaw, &pitch)) SH4R_Room302.pitch = pitch;
            InterlockedIncrement(&SH4R_Room302.lookTicks);
            return;
        }
        R3LookStep(&g_r3Ease, m, pd, yd, by, R3Dt(*(volatile const int *)0x5D5248), g_r3Feel, g_r3DownLim, g_r3UpLim, &o);
        ++g_r3V.look;
        g_r3V.eased += o.eased, g_r3V.clamped += o.clamped, g_r3V.up += o.up, g_r3V.down += o.down, g_r3V.rebuilt += o.rebuilt;
        if (o.rebuilt && LogQuota(&g_r3RebuildLogs, 5))
            LogV("room 302: look: the view was rolled or past a limit (pitch %.1f deg, roll %.2f): rebuilt at pitch %.1f, no roll",
                 o.fromDeg, o.rollDeg, o.pitch * 57.29578f);
        SH4R_Room302.yawRate = *yd, SH4R_Room302.pitchRate = *pd, SH4R_Room302.pitch = o.pitch;
        InterlockedIncrement(&SH4R_Room302.lookTicks);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_r3Fault = 1;  // fail-closed: the game's deltas from now on (the limit operands keep their values)
        if (LogQuota(&g_r3FaultLogs, 1)) LogAnomaly("room 302", "the look step faulted: Room 302's look is the game's from now on");
    }
}
static const DWORD g_r3LookApply = 0x52DDA0;
static __declspec(naked) void R3LookStub(void)  // 0x52DF74 (was `call 0x52DDA0`; ecx = the input, edi = the player)
{
    __asm {
        call dword ptr [g_r3LookApply]  // the original call (no stack arguments)
        pushad
        push edi
        call R3LookAfter
        add esp, 4
        popad
        ret
    }
}

// ---------------------------------------------------------------- who owns each group; the install
static int R3InstallLook(const char *when)
{
    GpPatch set[3];
    const DWORD up = (DWORD)(ULONG_PTR)&g_r3UpLim, down = (DWORD)(ULONG_PTR)&g_r3DownLim;
    GpBranch(&set[0], 0x52DF74, 0xE8, R3LookStub, 5);
    GpData(&set[1], 0x52DCC8, &up, 4);
    GpData(&set[2], 0x52DD16, &down, 4);
    if (!GpApplyAll(set, 3, "room 302 look")) return 0;
    g_r3LookHooked = 1;
    Log("room 302: look hooks in (%s): 0052DF74 call, 0052DCC6 / 0052DD14 operands", when);
    return 1;
}
static void R3Decide(int atTick, const char *when)
{
    static const char *const kG[R3G_N] = { "look", "walk" };
    static const unsigned kBit[R3G_N] = { R302F_LOOK, R302F_WALK };
    char pb[256];
    int g, wait = 0;
    R3Probe();
    for (g = 0; g < R3G_N; ++g) {
        R3OwnIn in;
        int a, own;
        memset(&in, 0, sizeof in);
        in.peMapped = g_r3Pe != NULL;
        in.yieldExp = g_r3Yield != NULL;
        in.yieldDecided = g_r3Yield && g_r3Yield->decided;
        in.yieldMask = g_r3Yield ? (unsigned)g_r3Yield->yield : 0u;
        in.peListsSite = g ? R3PeLists(kR3WalkVa[0], 6) || R3PeLists(kR3WalkVa[1], 6)
                           : R3PeLists(0x52DF74, 5) || R3PeLists(0x52DCC6, 6) || R3PeLists(0x52DD14, 6);
        in.sites = g ? R3WalkSiteState() : R3LookSiteState();
        in.avail = g ? g_moveOn : 1;
        in.atTick = atTick;
        own = R3DecideOwner(&in, kBit[g], &a);
        if (a)
            LogAnomaly("room 302", "%s: %s (Playable Eileen %s, its patch list %s the group's sites, the sites %s): nobody's hook is in",
                       kG[g], kR3OwnName[own], R3PeText(pb, sizeof pb), in.peListsSite ? "covers" : "does not cover",
                       in.sites == R3S_STOCK ? "stock" : in.sites == R3S_OURS ? "SH4 Refined's" : "another module's");
        if (own == R3_OWN_REFINED && !g && in.sites == R3S_STOCK && !R3InstallLook(when)) own = R3_OWN_NONE;
        if (own == R3_OWN_WAIT) wait = 1;
        InterlockedExchange(&g_r3Own[g], own);
    }
    g_r3Decided = !wait;
    InterlockedExchange(&SH4R_Room302.taken, (g_r3Own[R3G_LOOK] == R3_OWN_REFINED ? (LONG)R302F_LOOK : 0) |
                                             (g_r3Own[R3G_WALK] == R3_OWN_REFINED ? (LONG)R302F_WALK : 0));
    if (!wait) InterlockedExchange(&SH4R_Room302.decided, 1);
    R3Apply();
    {
        char t[R3K_N][16];
        int k;
        for (k = 0; k < R3K_N; ++k) R3IniText(k, g_r3Idx[k], t[k], sizeof t[k]);
        Log("room 302: owners (%s): look %s, walk %s; Playable Eileen %s; [Room302] LookDownLimit %s, LookUpLimit %s, LookFeel %s, "
            "WalkSpeed %s%s", when, kR3OwnName[g_r3Own[R3G_LOOK]], kR3OwnName[g_r3Own[R3G_WALK]], R3PeText(pb, sizeof pb), t[0], t[1],
            t[2], t[3], wait ? "; decided again on the first game tick" : "");
    }
}

// Init (DllMain thread), after GameplayInstall (Movement Speed's sites) and CameraInstall, before the Options tables
static void R3Install(void)
{
    static const char *const kLabel[R3K_N] = { "Look Down", "Look Up", "Look Feel", "Walk Speed" };
    static const char *const kDesc[R3K_N] = { "How far down the view goes. Game: 50 degrees.", "How far up the view goes. Game: 25 degrees.",
                                              "Smooths the turning. Off: the game's own.", "Room 302's walking speed. Game: the stock one." };
    static const struct { int id; const char *s; } kVal[] = {
        { R3M_GAME, "Game" }, { R3M_V35, "35" }, { R3M_V45, "45" }, { R3M_F05, "0.5" }, { R3M_F1, "1" }, { R3M_F15, "1.5" },
        { R3M_F2, "2" }, { R3M_W055, "0.55" }, { R3M_W06, "0.6" }, { R3M_W065, "0.65" }, { R3M_W07, "0.7" }, { R3M_W075, "0.75" },
        { R3M_W08, "0.8" }, { R3M_W09, "0.9" }, { R3M_W11, "1.1" }, { R3M_W12, "1.2" } };
    static const int kWalkId[R3_WALK_N] = { R3M_F05, R3M_W055, R3M_W06, R3M_W065, R3M_W07, R3M_W075, R3M_W08, R3M_W09, R3M_GAME,
                                            R3M_W11, R3M_W12 };
    static const int kFeelId[R3_FEEL_N] = { -1, R3M_F05, R3M_F1, R3M_F15, R3M_F2 };  // -1: the stock "Off" (MSG_ITEM0)
    int k, i;
    Encode(g_txtR3[R3M_ROW], sizeof g_txtR3[0], "Room 302");
    Encode(g_txtR3[R3M_ROWD], sizeof g_txtR3[0], "Look and walk in Room 302, Henry and Eileen.");
    Encode(g_txtR3[R3M_ROWD_PE], sizeof g_txtR3[0], "Playable Eileen sets these for now.");
    Encode(g_txtR3[R3M_TITLE], sizeof g_txtR3[0], "Presentation: Room 302");
    for (k = 0; k < R3K_N; ++k) {
        Encode(g_txtR3[R3M_L0 + k], sizeof g_txtR3[0], kLabel[k]);
        Encode(g_txtR3[R3M_D0 + k], sizeof g_txtR3[0], kDesc[k]);
    }
    Encode(g_txtR3[R3M_DPE], sizeof g_txtR3[0], "Playable Eileen does this for now.");
    Encode(g_txtR3[R3M_DNA], sizeof g_txtR3[0], "Not available (see SH4Refined.log).");
    for (i = 0; i < (int)(sizeof kVal / sizeof kVal[0]); ++i) Encode(g_txtR3[kVal[i].id], sizeof g_txtR3[0], kVal[i].s);
    // the value ids: Look Down / Up share 1st / 3rd Person FOV's number texts (60 .. 85), Look Feel the stock "Off"
    for (k = 0; k < 2; ++k)
        for (i = 0; i < kR3Count[k]; ++i) {
            const int deg = k ? kR3Up[i] : kR3Down[i];
            int f;
            DWORD id = MSG_R3 + (i ? R3M_DNA : R3M_GAME);
            if (deg == 35) id = MSG_R3 + R3M_V35;
            else if (deg == 45) id = MSG_R3 + R3M_V45;
            else if (i)
                for (f = 1; f < FOV_N; ++f)
                    if (kFovDeg[f] == deg) id = MSG_FOV0 + f;
            g_r3Ids[k][i] = id;
        }
    for (i = 0; i < R3_FEEL_N; ++i) g_r3Ids[R3K_FEEL][i] = kFeelId[i] < 0 ? MSG_ITEM0 : MSG_R3 + kFeelId[i];
    for (i = 0; i < R3_WALK_N; ++i) g_r3Ids[R3K_WALK][i] = MSG_R3 + kWalkId[i];
    sprintf_s(g_r3PeIni, sizeof g_r3PeIni, "%sSH4PlayableEileen.ini", g_dir);
    R3ReadIni(1);
    R3Decide(0, "at start");
}

// ---------------------------------------------------------------- the dispatcher tick (0x4EF982, game thread)
static float R3WalkK(void)
{
    return R3WalkFactor(g_r3Own[R3G_WALK] == R3_OWN_REFINED, (int)*(volatile DWORD *)0xFD5A90, g_r3Walk);
}
static void R3VisitFlush(const char *why)
{
    const DWORD s = (GetTickCount() - g_r3V.t0) / 1000;
    if ((g_r3V.look || g_r3V.walk) && LogQuota(&g_r3VisitLogs, 40))
        LogV("room 302: visit %lu s (%s): look %lu tick(s) (eased %lu, clamped %lu: up %lu, down %lu; view rebuilt %lu), walk x%g "
             "%lu tick(s)", s, why, g_r3V.look, g_r3V.eased, g_r3V.clamped, g_r3V.up, g_r3V.down, g_r3V.rebuilt,
             (double)(g_r3Own[R3G_WALK] == R3_OWN_REFINED ? g_r3Walk : 1.0f), g_r3V.walk);
    g_r3V.look = g_r3V.eased = g_r3V.clamped = g_r3V.up = g_r3V.down = g_r3V.rebuilt = g_r3V.walk = 0;
    g_r3V.t0 = g_r3V.tLog = GetTickCount();
}
static void R3Tick(void)
{
    __try {
        const int stage = (int)*(volatile DWORD *)0xFD5A90;
        float w;
        if (!g_r3Decided) R3Decide(1, "first game tick");
        w = 10.5f * g_moveK * R3WalkK();
        if (g_moveOn && g_r302Walk != w) g_r302Walk = w;  // (Movement Speed's own writes multiply by R3WalkK too)
        if (stage == 1) {
            if (!g_r3V.in) {
                g_r3V.in = 1;
                g_r3V.t0 = g_r3V.tLog = GetTickCount();
                g_r3V.look = g_r3V.eased = g_r3V.clamped = g_r3V.up = g_r3V.down = g_r3V.rebuilt = g_r3V.walk = 0;
            }
            if (R3WalkK() != 1.0f) ++g_r3V.walk;
            if (GetTickCount() - g_r3V.tLog > 300000) R3VisitFlush("5 minutes");
        } else if (g_r3V.in) {
            g_r3V.in = 0;
            R3VisitFlush("left");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (LogQuota(&g_r3FaultLogs, 1)) LogAnomaly("room 302", "the tick faulted");
    }
}

// ---------------------------------------------------------------- the rows (Option 4's folder, the page LP_R302)
static int R3RowLive(int k) { return k >= 0 && k < R3K_N && g_r3Own[kR3Group[k] == R302F_LOOK ? R3G_LOOK : R3G_WALK] == R3_OWN_REFINED; }
static int R3Step(int k, int count)
{
    int v;
    if (!R3RowLive(k)) {  // greyed: the buzzer on a press, nothing changes
        if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    v = StepValue(g_r3Idx[k], count);
    if (v >= 0) R3Set(k, v, "Options");
    return 0;
}
static int __cdecl R3In0(BYTE *cfg, int count) { (void)cfg; return R3Step(R3K_DOWN, count); }
static int __cdecl R3In1(BYTE *cfg, int count) { (void)cfg; return R3Step(R3K_UP, count); }
static int __cdecl R3In2(BYTE *cfg, int count) { (void)cfg; return R3Step(R3K_FEEL, count); }
static int __cdecl R3In3(BYTE *cfg, int count) { (void)cfg; return R3Step(R3K_WALK, count); }
static int __cdecl R3Get0(void) { return g_r3Idx[R3K_DOWN]; }
static int __cdecl R3Get1(void) { return g_r3Idx[R3K_UP]; }
static int __cdecl R3Get2(void) { return g_r3Idx[R3K_FEEL]; }
static int __cdecl R3Get3(void) { return g_r3Idx[R3K_WALK]; }
static int R3DescOf(int k)
{
    const int own = (int)g_r3Own[kR3Group[k] == R302F_LOOK ? R3G_LOOK : R3G_WALK];
    return MSG_R3 + (own == R3_OWN_REFINED ? R3M_D0 + k : own == R3_OWN_PE || own == R3_OWN_WAIT ? R3M_DPE : R3M_DNA);
}
static int __cdecl R3Desc0(void) { return R3DescOf(R3K_DOWN); }
static int __cdecl R3Desc1(void) { return R3DescOf(R3K_UP); }
static int __cdecl R3Desc2(void) { return R3DescOf(R3K_FEEL); }
static int __cdecl R3Desc3(void) { return R3DescOf(R3K_WALK); }
static int __cdecl R3DescFolder(void)
{
    return MSG_R3 + (g_r3Own[R3G_LOOK] == R3_OWN_REFINED || g_r3Own[R3G_WALK] == R3_OWN_REFINED ? R3M_ROWD : R3M_ROWD_PE);
}
static int __cdecl R3FolderIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_R302); }
// Option 4's table builder (sh4windowmod.c): the folder row at t + 7 * n (above Exit) and the page; the next free row
static int R3PresRow(DWORD *t, int n, const DWORD *stockRows)
{
    static const DWORD kBc[7] = { 0x524940, 0x51EB10, 0, 0x654C54, 1, 0x27, 0x523130 };  // Option 2's Button Config record
    static const void *const kIn[R3K_N] = { R3In0, R3In1, R3In2, R3In3 };
    static const void *const kGet[R3K_N] = { R3Get0, R3Get1, R3Get2, R3Get3 };
    static const void *const kDesc[R3K_N] = { R3Desc0, R3Desc1, R3Desc2, R3Desc3 };
    int k;
    g_r3Row = -1;
    if (memcmp((const void *)0x654D88, kBc, sizeof kBc)) {
        Log("room 302: Option 2's Button Config record is not the stock one: no Room 302 page ([Room302] in the ini still applies)");
        return n;
    }
    for (k = 0; k < R3K_N; ++k)
        FillRow(g_rowsR302 + 7 * k, kIn[k], kGet[k], g_r3Ids[k], kR3Count[k], MSG_R3 + R3M_L0 + k, kDesc[k]);
    GpBackRow(g_rowsR302, R3K_N, stockRows);
    g_lpTable[LP_R302] = g_rowsR302;
    g_lpCount[LP_R302] = R3K_N + 1;
    g_lpTitle[LP_R302] = MSG_R3 + R3M_TITLE;
    g_lpParent[LP_R302] = 3;
    g_r3Row = n;
    g_lpParentRow[LP_R302] = n;
    GpFolderRow(t + 7 * n, (const DWORD *)0x654D88, R3FolderIn, MSG_R3 + R3M_ROW, R3DescFolder);
    return n + 1;
}
// without the sub-screen fade the page cannot open: the folder row leaves Option 4 (it is the last row above Exit)
static void R3MenuOff(void)
{
    DWORD *t = g_lpTable[3];
    const int n = g_lpCount[3];
    if (g_r3Row < 0 || !t || g_r3Row >= n - 1) return;
    memmove(t + 7 * g_r3Row, t + 7 * (g_r3Row + 1), (size_t)(n - 1 - g_r3Row) * 7 * 4);
    t[7 * (n - 1)] = 0;
    g_lpCount[3] = n - 1;
    g_lpParent[LP_R302] = -1;
    g_r3Row = -1;
    Log("room 302: no sub-screen fade: no Room 302 page ([Room302] in the ini still applies)");
}
static int R3SubmenuRow(int row) { return g_r3Row >= 0 && row == g_r3Row; }  // the blue cue (sh4windowmod.c SubmenuRow, page 3)
static int R3RowGrey(int slot, int row)  // RowLabelC / RowValueC: a row of the page whose group is not Refined's
{
    return (unsigned)slot < 3 && g_slotPage[slot] == LP_R302 && row >= 0 && row < R3K_N && !R3RowLive(row);
}
// snapshot / changed / discard (ResetPages, PadChanged, RowChangedDraw, DiscardOwn)
static void R3Snapshot(void)
{
    int k;
    if (!g_iniFallback) R3ReadIni(0);  // an ini edit since: the rows show it
    for (k = 0; k < R3K_N; ++k) g_r3Snap[k] = g_r3Idx[k];
}
static int R3RowChanged(int row) { return g_snapOn && row >= 0 && row < R3K_N && g_r3Idx[row] != g_r3Snap[row]; }
static int R3Changed(void)
{
    int k;
    for (k = 0; k < R3K_N; ++k)
        if (R3RowChanged(k)) return 1;
    return 0;
}
static void R3Discard(void)
{
    int k;
    if (!g_snapOn) return;
    for (k = 0; k < R3K_N; ++k)
        if (g_r3Idx[k] != g_r3Snap[k]) R3Set(k, g_r3Snap[k], "discarded");
}
