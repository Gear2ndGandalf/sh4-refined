// ================================================================ v3.48 RZ348: the Randomizer (Option 5 > Randomizer)
// Plan: staging/v348_feedback/w3_rando/PLAN.md (the research: staging/research_randomizer/REPORT.md sections 1, 4, 7;
// round 2: weapons, the look, Change Each Visit). Included by sh4windowmod.c after regen.inc (it needs gameplay.inc's lock,
// rows and texts); enemies.inc's EnmWrap (the re-aimed `call 52CA00` at 0x4F0AF8, the room load) calls RzBeforeScript /
// RzAfterScript right around the stock room script runner 0x52CA00 (v3.48 fix1: in a __try / __finally, a word list per
// run). No exe code is patched: before the runner reads this
// room's script, the item word of each pickup in the pool (rando_sim.h: the 137 plain consumable pickups and the 27
// weapon spots) is written with the chosen item, and a weapon spot's record word with the record that offers and gives
// that item (0 = the default record and the item's own pickup script); right after the runner every word gets its stock
// value back (nothing of ours is left in .data after the room's build). The pickup objects keep what they were built
// with: the model follows the item word, the prompt and the GIVE the record / default script (rando_sim.h, the facts).
// The taken bit stays the slot's (stage, room, bit): saves are untouched, a taken slot stays taken.
// Change Each Visit On (default): Random draws anew at every room visit (a per-room visit count, in memory only), Shuffle
// at every world visit (a world-visit count): a room keeps its Shuffle items while you stay in the world. Off: v1's fixed
// items (a pure function of the seed, the stage, the settings and the stock scripts).
// [Randomizer] Items Off / Shuffle / Random, KeepSpecials On / Off, Room302 In / Out, PerVisit On / Off, Seed (decimal;
// missing or 0 = rolled once and written). Later rounds (the folder has room for their rows): Monsters, Ghosts, Looks.
#include "rando_sim.h"

static int g_enmInstalled;  // enemies.inc (a tentative definition: the same object): its room-load hook 0x4F0AF8 is in
enum { RZS_ITEMS, RZS_KEEP, RZS_R302, RZS_SEED, RZS_PERVISIT, RZS_N };  // the settings (g_rzSet, g_rzSnap)
static volatile LONG g_rzItems = RZ_ITEMS_DEFAULT, g_rzKeep = RZ_KEEP_DEFAULT, g_rzR302 = RZ_R302_DEFAULT;
static volatile LONG g_rzSeed;  // (an unsigned seed; never 0 once read)
static volatile LONG g_rzPerVisit = RZ_PERVISIT_DEFAULT;  // round 2: Change Each Visit
static LONG g_rzSnap[RZS_N];    // as Options opened
static int g_rzUnsaved;         // the menu's value could not be written (the poll leaves the file's value alone)
static int g_rzFail;            // faulted: off for this session
static int g_rzSelfN = -1, g_rzSelfW;  // the start's scan: plain pickups / weapon spots found (-1 = not scanned)
static int g_rzSelfBad;         // world visits whose scripts are not the stock ones (left alone)
static int g_rzBadSaid[RZ_STAGES][2];
static RzWeapons g_rzWt;        // round 2: the weapons a spot may take (built at the start from every script)
static int g_rzWtOn;            // the weapon table is built (else the weapon spots stay, no weapon is drawn)
static int g_rzOwnedOk;         // ItemIsObtained 0x4F69C0 is the stock code (its byte array 0x13D7FF8 is read)
// the stage in hand: its scans, its plan
static RzScan g_rzScan[2];
static int g_rzScanStage = -1, g_rzScanHas1, g_rzVisitOk[2];
static unsigned char g_rzOut[2][RZ_VMAX], g_rzPool[2][RZ_VMAX];
static unsigned g_rzRec[2][RZ_VMAX];
static RzPlanStat g_rzStat[2];
static unsigned g_rzPlanKey;
static int g_rzPlanSaid;
// round 2: the visit counts (memory only, never saved): a room's visits by (stage, room), the world visits
static unsigned g_rzRoomVisit[RZ_STAGES][RZ_ROOMS];  // v3.48 fix1 RZ-003: 32 bit (was 16, wrapped to 1)
static unsigned g_rzWorldVisit;
static int g_rzLastStage = -1;
// the words written for the room being built (an item word and a record word a pickup at most)
static RzPatchStack g_rzStack;  // v3.48 fix1 RZ-002: one word list per (nested) room-script run (rando_sim.h)
static int g_rzTakenOk;         // v3.48 fix1 RZ-001: PickupTakenGetBits 0x4F3B90 is the stock code (gPickupTaken 0x1078618)
// the session's counts (the session summary)
static int g_rzLoads, g_rzRoomsPooled, g_rzChanged, g_rzWeapons, g_rzPlans, g_rzLeft, g_rzFirstSaid;
static volatile LONG g_rzQuotaAnom, g_rzQuotaRoom;

// ---------------------------------------------------------------- texts (the MSG_RZ0 block, 32 ids)
enum { RZM_T, RZM_R, RZM_DF, RZM_LI, RZM_VI0, RZM_DI0 = RZM_VI0 + RZI_N, RZM_LK = RZM_DI0 + RZI_N, RZM_VK0, RZM_DK0 = RZM_VK0 + 2,
       RZM_LR = RZM_DK0 + 2, RZM_VR0, RZM_DR0 = RZM_VR0 + 2, RZM_LS = RZM_DR0 + 2, RZM_DS, RZM_LN, RZM_DN, RZM_NA,
       RZM_LP, RZM_DP0, RZM_N = RZM_DP0 + 2 };  // round 2: + Change Each Visit's label and its two lines
typedef char RzTextsFit[RZM_N <= MSG_END - MSG_RZ0 ? 1 : -1];  // the MSG_RZ0 block (32 ids; 28 used)
static BYTE g_txtRz[RZM_N][64];
static DWORD g_rzIdsItems[RZI_N], g_rzIdsKeep[2], g_rzIdsR302[2];
static const BYTE *RzText(int id)
{
    return id >= MSG_RZ0 && id < MSG_RZ0 + RZM_N ? g_txtRz[id - MSG_RZ0] : g_txtRz[RZM_NA];
}
static void RzSeedLabel(void)
{
    char s[40];
    sprintf_s(s, sizeof s, "Seed %lu", (unsigned long)(unsigned)g_rzSeed);
    Encode(g_txtRz[RZM_LS], 64, s);
}
static void RzTexts(void)
{
    int k;
    Encode(g_txtRz[RZM_T], 64, "Gameplay: Randomizer");
    Encode(g_txtRz[RZM_R], 64, "Randomizer");
    Encode(g_txtRz[RZM_DF], 64, "Randomize the item and weapon pickups.");
    Encode(g_txtRz[RZM_LI], 64, "Items");
    for (k = 0; k < RZI_N; ++k) {
        Encode(g_txtRz[RZM_VI0 + k], 64, kRzItemsName[k]);
        g_rzIdsItems[k] = MSG_RZ0 + RZM_VI0 + k;
    }
    Encode(g_txtRz[RZM_DI0 + RZI_OFF], 64, "Pickups as the game places them. Next door.");
    Encode(g_txtRz[RZM_DI0 + RZI_SHUFFLE], 64, "Shuffle: a world's items swap spots. Next door.");
    Encode(g_txtRz[RZM_DI0 + RZI_RANDOM], 64, "Random: every spot draws an item. Next door.");
    Encode(g_txtRz[RZM_LK], 64, "Keep Specials");
    for (k = 0; k < 2; ++k) {
        Encode(g_txtRz[RZM_VK0 + k], 64, kRzKeepName[k]);
        g_rzIdsKeep[k] = MSG_RZ0 + RZM_VK0 + k;
        Encode(g_txtRz[RZM_VR0 + k], 64, kRzR302Name[k]);
        g_rzIdsR302[k] = MSG_RZ0 + RZM_VR0 + k;
    }
    Encode(g_txtRz[RZM_DK0], 64, "Off: Candles, Medallions, Swords mix in too.");
    Encode(g_txtRz[RZM_DK0 + 1], 64, "On: the specials stay in place. Next door.");
    Encode(g_txtRz[RZM_LR], 64, "Room 302");
    Encode(g_txtRz[RZM_DR0], 64, "Out: Room 302's pickups stay. Next door.");
    Encode(g_txtRz[RZM_DR0 + 1], 64, "In: Room 302's pickups change too. Next door.");
    RzSeedLabel();
    Encode(g_txtRz[RZM_DS], 64, "Every choice comes from it (not set here).");
    Encode(g_txtRz[RZM_LN], 64, "New Seed");
    Encode(g_txtRz[RZM_DN], 64, "Enter rolls a new seed. Next door.");
    Encode(g_txtRz[RZM_NA], 64, "Not available in this game (see SH4Refined.log).");
    Encode(g_txtRz[RZM_LP], 64, "Change Each Visit");
    Encode(g_txtRz[RZM_DP0], 64, "Off: a room keeps its items for this seed.");
    Encode(g_txtRz[RZM_DP0 + 1], 64, "On: new items at every visit. Next door.");
}

// ---------------------------------------------------------------- [Randomizer]
static int RzAvail(void) { return g_enmInstalled && !g_rzFail; }
static volatile LONG *RzSetting(int which)
{
    switch (which) {
    case RZS_ITEMS: return &g_rzItems;
    case RZS_KEEP: return &g_rzKeep;
    case RZS_R302: return &g_rzR302;
    case RZS_SEED: return &g_rzSeed;
    }
    return &g_rzPerVisit;
}
static const char *const kRzKey[RZS_N] = { "Items", "KeepSpecials", "Room302", "Seed", "PerVisit" };
static const char *RzSeedStr(unsigned s, char *b, size_t n)
{
    sprintf_s(b, n, "%lu", (unsigned long)s);
    return b;
}
static const char *RzValName(int which, LONG v, char *b, size_t n)
{
    if (which == RZS_ITEMS) return v >= 0 && v < RZI_N ? kRzItemsName[v] : "?";
    if (which == RZS_R302) return kRzR302Name[v ? 1 : 0];
    if (which == RZS_SEED) return RzSeedStr((unsigned)v, b, n);
    return kRzKeepName[v ? 1 : 0];
}
static int RzWrite(const char *key, const char *val)  // 1 = written
{
    if (g_iniFallback) return 0;
    if (WritePrivateProfileStringA("Randomizer", key, val, g_iniPath)) return 1;
    Log("randomizer: [Randomizer] %s %s applies now but could not be saved in SH4Refined.ini (error %lu)", key, val, GetLastError());
    return 0;
}
static unsigned RzNewSeed(unsigned prev)
{
    LARGE_INTEGER q;
    unsigned s;
    QueryPerformanceCounter(&q);
    s = RzSeedRoll(prev ^ GetTickCount(), q.LowPart ^ (unsigned)q.HighPart, GetCurrentProcessId() ^ GetCurrentThreadId());
    return s == prev ? RzSeedRoll(s, 1, 2) : s;
}
// one key's text -> value (def when missing or unknown; unknown is logged)
static int RzReadKey(const char *key, int (*parse)(const char *), int def, const char *const *names, const char *what)
{
    char b[64];
    int v;
    IniStr("Randomizer", key, b, sizeof b);
    v = parse(b);
    if (v < 0) {
        Log("randomizer: [Randomizer] %s \"%s\" is not %s: %s (the default)", key, b, what, names[def]);
        return def;
    }
    return v;
}
static void RzReadIni(void)
{
    char b[64], s[16];
    unsigned seed;
    g_rzItems = RzReadKey("Items", RzItemsOfText, RZ_ITEMS_DEFAULT, kRzItemsName, "Off / Shuffle / Random");
    g_rzKeep = RzReadKey("KeepSpecials", RzOnOffOfText, RZ_KEEP_DEFAULT, kRzKeepName, "On / Off");
    g_rzR302 = RzReadKey("Room302", RzR302OfText, RZ_R302_DEFAULT, kRzR302Name, "In / Out");
    g_rzPerVisit = RzReadKey("PerVisit", RzOnOffOfText, RZ_PERVISIT_DEFAULT, kRzKeepName, "On / Off");  // round 2
    IniStr("Randomizer", "Seed", b, sizeof b);
    seed = RzSeedOfText(b);
    if (!seed) {
        seed = RzNewSeed(0);
        if (RzWrite("Seed", RzSeedStr(seed, s, sizeof s)))
            Log("randomizer: [Randomizer] Seed was \"%s\" (missing, 0 or not a number): rolled %s and written", b, s);
        else
            Log("randomizer: [Randomizer] Seed was \"%s\": rolled %s for this session (not written)", b, s);
    }
    g_rzSeed = (LONG)seed;
    RzSeedLabel();
}
static void RzSetLocked(int which, LONG v, const char *why)  // which: RZS_* (GpLock held)
{
    char s[16], w[16];
    volatile LONG *const t = RzSetting(which);
    const LONG was = *t;
    const char *val = RzValName(which, v, s, sizeof s);
    InterlockedExchange(t, v);
    if (which == RZS_SEED) RzSeedLabel();
    g_rzUnsaved = !RzWrite(kRzKey[which], val) && !g_iniFallback;
    if (which == RZS_SEED)
        Log("randomizer: %s %s (was %s; %s; applies at the next room load)", why, val, RzSeedStr((unsigned)was, w, sizeof w),
            g_rzUnsaved ? "NOT saved" : "saved");
    else
        Log("randomizer: %s %s (was %s; applies at the next room load)", kRzKey[which], val, RzValName(which, was, w, sizeof w));
}
static void RzIniReread(void)  // under GpLock (gameplay.inc GpIniReread)
{
    static const int kWhich[4] = { RZS_ITEMS, RZS_KEEP, RZS_R302, RZS_PERVISIT };
    int (*const parse[4])(const char *) = { RzItemsOfText, RzOnOffOfText, RzR302OfText, RzOnOffOfText };
    char b[64], s[16];
    int k, v;
    unsigned seed;
    if (g_iniFallback || g_rzUnsaved) return;
    for (k = 0; k < 4; ++k) {
        volatile LONG *const t = RzSetting(kWhich[k]);
        GetPrivateProfileStringA("Randomizer", kRzKey[kWhich[k]], "", b, sizeof b, g_iniPath);
        v = parse[k](b);
        if (v < 0 || v == (int)*t) continue;  // (missing or unknown text: the value in force stays)
        InterlockedExchange(t, v);
        g_rzSnap[kWhich[k]] = v;
        Log("randomizer: [Randomizer] %s read again: %s (applies at the next room load)", kRzKey[kWhich[k]],
            RzValName(kWhich[k], v, s, sizeof s));
    }
    GetPrivateProfileStringA("Randomizer", "Seed", "", b, sizeof b, g_iniPath);
    seed = RzSeedOfText(b);
    if (seed && seed != (unsigned)g_rzSeed) {
        InterlockedExchange(&g_rzSeed, (LONG)seed);
        g_rzSnap[RZS_SEED] = (LONG)seed;
        RzSeedLabel();
        Log("randomizer: [Randomizer] Seed read again: %lu (applies at the next room load)", (unsigned long)seed);
    }
}
static void RzSnapshot(void)
{
    int k;
    for (k = 0; k < RZS_N; ++k) g_rzSnap[k] = *RzSetting(k);
}
static int RzChanged(void)
{
    int k;
    for (k = 0; k < RZS_N; ++k)
        if (*RzSetting(k) != g_rzSnap[k]) return 1;
    return 0;
}
static void RzDiscard(void)  // under GpLock (gameplay.inc GpDiscard): "No" at the exit puts the snapshot back
{
    int k;
    for (k = 0; k < RZS_N; ++k)
        if (*RzSetting(k) != g_rzSnap[k]) RzSetLocked(k, g_rzSnap[k], "Seed back to");
}

// ---------------------------------------------------------------- the page (LP_RANDO)
static int g_rzRow[RZS_N] = { -1, -1, -1, -1, -1 }, g_rzRowNew = -1, g_rzRowsN;
static int RzStep(int which, int count)
{
    int v;
    if (!RzAvail()) {
        if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue((int)*RzSetting(which), count);
    if (v >= 0) RzSetLocked(which, v, "");
    GpUnlock();
    return 0;
}
static int __cdecl RzInItems(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_ITEMS, RZI_N); }
static int __cdecl RzInKeep(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_KEEP, 2); }
static int __cdecl RzInR302(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_R302, 2); }
static int __cdecl RzInPerVisit(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_PERVISIT, 2); }  // round 2
static int __cdecl RzInSeed(BYTE *cfg, int count)  // shown, not edited: the buzzer on a press
{
    (void)cfg;
    (void)count;
    if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
    return 0;
}
static int __cdecl RzInNew(BYTE *cfg, int count)  // Enter: a new seed, written at once, shown on the Seed row
{
    (void)cfg;
    (void)count;
    if (!((Fn2)0x4F84A0)(0, 0x11)) return 0;
    if (!RzAvail()) {
        ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    RzSetLocked(RZS_SEED, (LONG)RzNewSeed((unsigned)g_rzSeed), "New Seed");
    GpUnlock();
    ((Fn1)0x56CC40)(0x9C43);
    return 0;
}
static int __cdecl RzGetItems(void) { return (int)g_rzItems >= 0 && g_rzItems < RZI_N ? (int)g_rzItems : 0; }
static int __cdecl RzGetKeep(void) { return g_rzKeep ? 1 : 0; }
static int __cdecl RzGetR302(void) { return g_rzR302 ? 1 : 0; }
static int __cdecl RzGetPerVisit(void) { return g_rzPerVisit ? 1 : 0; }
static int __cdecl RzDescItems(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DI0 + RzGetItems()); }
static int __cdecl RzDescKeep(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DK0 + RzGetKeep()); }
static int __cdecl RzDescR302(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DR0 + RzGetR302()); }
static int __cdecl RzDescPerVisit(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DP0 + RzGetPerVisit()); }
static int __cdecl RzDescSeed(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DS); }
static int __cdecl RzDescNew(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DN); }
static int __cdecl RzDescFolder(void) { return MSG_RZ0 + RZM_DF; }
static int __cdecl RzFolderIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_RANDO); }
// Option 5's folder row (gameplay.inc GpBuildPages, under Special) and the page: Items, Keep Specials, Room 302, Change
// Each Visit, Seed, New Seed, Back. bcRow = Option 2's Button Config record (value-less rows: the Seed shows its number in
// the label).
static void RzFolderRow(DWORD *r, const DWORD *bcRow) { GpFolderRow(r, bcRow, RzFolderIn, MSG_RZ0 + RZM_R, RzDescFolder); }
static void RzBuildPage(const DWORD *stockRows, const DWORD *bcRow, DWORD *t, int parentRow)
{
    int n = 0;
    g_rzRow[RZS_ITEMS] = n;
    FillRow(t + 7 * n++, RzInItems, RzGetItems, g_rzIdsItems, RZI_N, MSG_RZ0 + RZM_LI, RzDescItems);
    g_rzRow[RZS_KEEP] = n;
    FillRow(t + 7 * n++, RzInKeep, RzGetKeep, g_rzIdsKeep, 2, MSG_RZ0 + RZM_LK, RzDescKeep);
    g_rzRow[RZS_R302] = n;
    FillRow(t + 7 * n++, RzInR302, RzGetR302, g_rzIdsR302, 2, MSG_RZ0 + RZM_LR, RzDescR302);
    g_rzRow[RZS_PERVISIT] = n;  // round 2
    FillRow(t + 7 * n++, RzInPerVisit, RzGetPerVisit, g_rzIdsKeep, 2, MSG_RZ0 + RZM_LP, RzDescPerVisit);
    g_rzRow[RZS_SEED] = n;
    GpFolderRow(t + 7 * n++, bcRow, RzInSeed, MSG_RZ0 + RZM_LS, RzDescSeed);
    g_rzRowNew = n;
    GpFolderRow(t + 7 * n++, bcRow, RzInNew, MSG_RZ0 + RZM_LN, RzDescNew);
    g_rzRowsN = n;
    GpBackRow(t, n, stockRows);
    g_lpTable[LP_RANDO] = t;
    g_lpCount[LP_RANDO] = n + 1;
    g_lpTitle[LP_RANDO] = MSG_RZ0 + RZM_T;
    g_lpParent[LP_RANDO] = 4;
    g_lpParentRow[LP_RANDO] = parentRow;
}
static int RzRowChanged(int row)  // the yellow mark (gameplay.inc GpRowChanged, LP_RANDO)
{
    int k;
    if (!g_snapOn || row < 0) return 0;
    for (k = 0; k < RZS_N; ++k)
        if (row == g_rzRow[k]) return *RzSetting(k) != g_rzSnap[k];
    return 0;
}

// ---------------------------------------------------------------- the scan and the plan (game thread, the room load)
static const RzWin *RzWindow(void)  // .data from the pickup records / scripts / item tables to the end of the room scripts
{
    static RzWin w;
    w.base = (const unsigned char *)(ULONG_PTR)RZ_DATA_LO, w.lo = RZ_DATA_LO, w.hi = RZ_SCRIPT_HI;
    return &w;
}
static unsigned RzScript(int stage, int visit)
{
    return *(const DWORD *)(ULONG_PTR)((visit ? 0x6553D8u : 0x6553A4u) + 4u * (unsigned)stage);
}
static void RzScanStage(int stage)  // (no word of ours is in the scripts now: RzAfterScript restored them)
{
    const unsigned p0 = RzScript(stage, 0), p1 = RzScript(stage, 1);
    int v;
    g_rzScanStage = stage;
    g_rzScanHas1 = p1 != p0;
    for (v = 0; v < 1 + g_rzScanHas1; ++v) {
        const unsigned p = v ? p1 : p0;
        int m;
        RzScanScript(RzWindow(), p, &g_rzScan[v]);
        m = RzStockMatch(stage, v, p, &g_rzScan[v]);
        g_rzVisitOk[v] = m == 1 || (m < 0 && !g_rzScan[v].n && !g_rzScan[v].bad);  // (not in the table: fine without pickups)
        if (!g_rzVisitOk[v] && !g_rzBadSaid[stage][v]++)
            LogAnomaly("randomizer", "stage %d visit %d: the script at %08X is not the stock one (%d plain pickup(s), %d weapon "
                       "spot(s)%s): its pickups are left alone", stage, v, p, g_rzScan[v].nPlain, g_rzScan[v].nWeapon,
                       g_rzScan[v].bad ? ", unreadable" : "");
    }
    if (!g_rzScanHas1) g_rzVisitOk[1] = 0;
}
static void RzPlanLog(int stage, int v, int mode, int perVisit)
{
    const RzPlanStat *s = &g_rzStat[v];
    char sp[200];
    int o = 0, k;
    static const int kSp[3] = { RZ_ITEM_CANDLE, RZ_ITEM_MEDALLION, RZ_ITEM_SWORD };
    if (!s->pooled) return;
    for (k = 0; k < 3; ++k)
        o += sprintf_s(sp + o, sizeof sp - o, "%s%s %d (stock %d)", k ? ", " : "", RzItemName(kSp[k]), s->cnt[kSp[k]], s->stock[kSp[k]]);
    Log("randomizer: stage %d visit %d: %s, %d of %d pickup(s) in the pool (%d weapon spot(s)), %d changed (seed %lu, Keep "
        "Specials %s, Room 302 %s, Change Each Visit %s, world visit %u); %s, weapons %d (stock %d)%s%s", stage, v,
        kRzItemsName[mode], s->pooled, g_rzScan[v].n, g_rzScan[v].nWeapon, s->changed, (unsigned long)(unsigned)g_rzSeed,
        kRzKeepName[g_rzKeep ? 1 : 0], kRzR302Name[g_rzR302 ? 1 : 0], kRzKeepName[perVisit ? 1 : 0], perVisit ? g_rzWorldVisit : 0,
        sp, s->weapons, s->weaponsStock,
        s->cnt[RZ_ITEM_SWORD] < s->stock[RZ_ITEM_SWORD] ? "; fewer Swords of Obedience than the stock (no guarantee)" : "",
        s->unplaced ? "; SOME ITEMS COULD NOT BE PLACED (they keep their spots)" : "");
}
static unsigned long long RzOwned(void)  // bit (item - 0x10): the weapon was obtained (ItemIsObtained's array 0x13D7FF8)
{
    unsigned long long m = 0;
    int i;
    if (!g_rzOwnedOk) return 0;
    for (i = 0x10; i <= 0x35; ++i)
        if (*(volatile const BYTE *)(ULONG_PTR)(0x13D7FF8u + (unsigned)i)) m |= 1ull << (i - 0x10);
    return m;
}
// the plan for this room load: Shuffle is the same within a world visit (fixed: always), Random the room's visit
static void RzPlanNow(int stage, int mode)
{
    const int keep = g_rzKeep != 0, r302 = g_rzR302 != 0, perVisit = g_rzPerVisit != 0;
    const unsigned seed = (unsigned)g_rzSeed;
    RzPlanIn in;
    unsigned key;
    int v;
    if (stage != g_rzScanStage) RzScanStage(stage);
    in.mode = mode, in.keep = keep, in.r302 = r302, in.share = !perVisit, in.seed = seed;
    in.worldSalt = perVisit ? g_rzWorldVisit : 0;
    in.roomSalt = perVisit ? g_rzRoomVisit[stage] : NULL;
    in.owned = mode == RZI_RANDOM ? RzOwned() : 0;
    in.taken = g_rzTakenOk && stage >= 0 && stage < RZ_STAGES ?  // v3.48 fix1 RZ-001: gPickupTaken[stage] (50 rooms x 8 bytes)
               (const unsigned char *)(ULONG_PTR)(0x1078618u + (unsigned)stage * (RZ_TAKEN_ROOMS * RZ_TAKEN_SLOTS / 8)) : NULL;
    in.wt = g_rzWtOn ? &g_rzWt : NULL;
    RzPlanStage(&in, stage, &g_rzScan[0], g_rzScanHas1 ? &g_rzScan[1] : NULL, g_rzOut[0], g_rzPool[0], g_rzRec[0], &g_rzStat[0],
                g_rzOut[1], g_rzPool[1], g_rzRec[1], &g_rzStat[1]);
    ++g_rzPlans;
    // one plan line per world visit / settings change (not per room: with Change Each Visit Random plans every room load)
    key = RzHash4(seed, (unsigned)stage, (unsigned)mode | (unsigned)keep << 4 | (unsigned)r302 << 5 | (unsigned)perVisit << 6,
                  in.worldSalt ^ 0x504C414Eu);
    if (g_rzPlanSaid && key == g_rzPlanKey) return;
    g_rzPlanKey = key, g_rzPlanSaid = 1;
    for (v = 0; v < 1 + g_rzScanHas1; ++v)
        if (g_rzVisitOk[v]) RzPlanLog(stage, v, mode, perVisit);
}

// every item's pickup model must be resident: item_model.bin (0xF000F003; items 1-4, 8-0x0B) and wp_model.bin
// (0xF000F057; the Sword's 0xF04CF057 and every weapon that moves) are in the world list (index 0) of stages 1..8, both
// halves (tools/rz_scan.py checked the stock exe: stages 9..11 lack item_model.bin, 10 both; they have no pickups here)
static int RzModelsListed(int stage)
{
    const DWORD *const *tab;
    const DWORD *l;
    int k, i, a = 0, b = 0;
    if (stage < 0 || stage >= RZ_STAGES) return 0;
    k = stage + ((*(const BYTE *)0x13D812D & 4) ? 13 : 0);  // (enemies.inc EnmResident's rule)
    tab = *(const DWORD *const *const *)(ULONG_PTR)(0x62D514 + 4 * k);
    if (!tab || !(l = tab[0])) return 0;
    for (i = 0; i < 80 && l[i] != 0x0FFFFFFF; ++i) a |= l[i] == 0xF000F003u, b |= l[i] == 0xF000F057u;
    return a && b;
}
// ---------------------------------------------------------------- the room load (enemies.inc EnmWrap)
// v3.48 fix1 RZ-002: the words of this run go into its own level d of the patch stack
static int RzAddPatch(int d, unsigned va, unsigned stock, unsigned now)  // 1 = written
{
    return RzStackAdd(&g_rzStack, d, (unsigned *)(ULONG_PTR)va, stock, now);
}
static void RzBeforeScriptC(int room, int d)
{
    const int mode = (int)g_rzItems, stage = (int)*(volatile DWORD *)0xFD5A90;
    unsigned start, rk = 0;
    int visit, i, pooled = 0, changed = 0, other = 0, weapons = 0;
    char list[700];
    int o = 0;
    ++g_rzLoads;
    if (!g_rzFirstSaid++)
        Log("randomizer: the first room load through 004F0AF8 (stage %d room %d): Items %s", stage, room,
            mode >= 0 && mode < RZI_N ? kRzItemsName[mode] : "?");
    if (stage < 0 || stage >= RZ_STAGES || room <= 0 || room >= RZ_ROOMS) return;
    if (g_rzPerVisit) {  // round 2: the visit counts (also while Items is Off: switching on later gives a fresh draw)
        if (stage != g_rzLastStage) ++g_rzWorldVisit;
        rk = g_rzRoomVisit[stage][room] = RzVisitNext(g_rzRoomVisit[stage][room]);  // v3.48 fix1 RZ-003: 32 bit, stops at the top
    }
    g_rzLastStage = stage;
    if (mode <= RZI_OFF || mode >= RZI_N) return;
    RzPlanNow(stage, mode);
    start = ((const DWORD *)0x10A0A78)[room];
    visit = RzVisitOfRoom(&g_rzScan[0], g_rzScanHas1 ? &g_rzScan[1] : NULL, room, start);
    if (visit < 0) {
        if (start && LogQuota(&g_rzQuotaAnom, 20))
            LogAnomaly("randomizer", "room %d:%d: its script (%08X) is in neither half's scan: its pickups are left alone",
                       stage, room, start);
        return;
    }
    if (!g_rzVisitOk[visit]) return;
    if (!RzModelsListed(stage)) {
        if (!g_rzBadSaid[stage][visit]++)
            LogAnomaly("randomizer", "stage %d: item_model.bin / wp_model.bin are not both in the world's file list: its pickups are "
                       "left alone", stage);
        return;
    }
    list[0] = 0;
    for (i = 0; i < g_rzScan[visit].n; ++i) {
        const RzSlot *s = &g_rzScan[visit].s[i];
        const int now = g_rzOut[visit][i];
        if (s->room != room || !g_rzPool[visit][i]) continue;
        ++pooled;
        weapons += RzIsHenryWeapon(now);
        if (now != s->item) {
            const int n0 = d >= 0 && d < RZ_DEPTH ? g_rzStack.n[d] : 0;
            // a weapon spot: its record word first (the item word only with it), so a pickup never offers one thing and
            // shows another; a plain spot has no record word (its item's default script follows the item word)
            if (s->kind == RZK_WEAPON && g_rzRec[visit][i] != s->rec && !RzAddPatch(d, s->recVa, s->rec, g_rzRec[visit][i])) {
                ++other;
            } else if (!RzAddPatch(d, s->va, s->item, (unsigned)now)) {
                RzStackTrim(&g_rzStack, d, n0);  // (the record word back too)
                ++other;
            } else {
                ++changed;
            }
        }
        if (o < (int)sizeof list - 90)
            o += sprintf_s(list + o, sizeof list - o, "%sbit %d %s%s%s", o ? "; " : "", s->taken, RzItemName(s->item),
                           now != s->item ? " -> " : " (kept)", now != s->item ? RzItemName(now) : "");
    }
    if (!pooled) return;
    ++g_rzRoomsPooled;
    g_rzChanged += changed;
    g_rzWeapons += weapons;
    g_rzLeft += other;
    if (LogQuota(&g_rzQuotaRoom, 4000))
        Log("randomizer: room %d:%d (visit %d, #%u): %d of %d pickup(s) changed, %d weapon(s) (Items %s)%s", stage, room, visit, rk,
            changed, pooled, weapons, kRzItemsName[mode], other ? "; some words were not the stock value: left as found" : "");
    if (other && LogQuota(&g_rzQuotaAnom, 20))
        LogAnomaly("randomizer", "room %d:%d: %d pickup word(s) did not hold the stock value (another mod?): left as found", stage,
                   room, other);
    LogV("randomizer: room %d:%d: %s", stage, room, list);
}
// v3.48 fix1 RZ-002: RzBeforeScript returns this run's level; enemies.inc EnmWrap runs the stock runner in a __try and
// RzAfterScript(level) in its __finally, so the words go back even when the runner's fault is handled further out, and a
// run inside a run (a room built during another room's build) keeps the outer run's list (its own level)
static int __cdecl RzBeforeScript(int room)
{
    const int d = RzStackPush(&g_rzStack);
    if (d >= RZ_DEPTH) {
        if (LogQuota(&g_rzQuotaAnom, 20))
            LogAnomaly("randomizer", "room %d: a room build %d deep inside other room builds: nothing randomized in it", room, d + 1);
        return d;
    }
    if (d > 0 && LogQuota(&g_rzQuotaAnom, 20))
        LogAnomaly("randomizer", "room %d: a room build inside another room build (level %d): its own word list", room, d + 1);
    if (g_rzFail) return d;
    __try {
        RzBeforeScriptC(room, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        int left = 0;
        __try {
            left = RzStackTrim(&g_rzStack, d, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        g_rzFail = 1;
        LogAnomaly("randomizer", "faulted at a room load (code %08lX; %d word(s) not restored): off for this session",
                   GetExceptionCode(), left);
    }
    return d;
}
static void __cdecl RzAfterScript(int d)
{
    int other = 0;
    __try {
        other = RzStackPop(&g_rzStack, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rzStack.depth = d < 0 ? 0 : d;
        g_rzFail = 1;
        LogAnomaly("randomizer", "faulted restoring the pickup words (code %08lX): off for this session", GetExceptionCode());
    }
    if (other && LogQuota(&g_rzQuotaAnom, 20))
        LogAnomaly("randomizer", "%d pickup word(s) changed while the room was built (another writer): left as found", other);
}

// ---------------------------------------------------------------- start and end
static void RzInit(void)  // gameplay.inc GameplayInstall (before the Options tables)
{
    static const BYTE kObtained[26] = { 0x8B, 0x4C, 0x24, 0x04, 0x33, 0xC0, 0x85, 0xC9, 0x7E, 0x0F, 0x81, 0xF9, 0x88, 0x00, 0x00,
                                        0x00, 0x7D, 0x07, 0x0F, 0xB6, 0x81, 0xF8, 0x7F, 0x3D, 0x01, 0xC3 };  // ItemIsObtained 0x4F69C0
    static const BYTE kTakenBits[6] = { 0xB8, 0x18, 0x86, 0x07, 0x01, 0xC3 };  // v3.48 fix1 RZ-001: PickupTakenGetBits 0x4F3B90
    int stage, v, total = 0, wtotal = 0, scripts = 0, wn = 0, i;
    unsigned seen[RZ_STAGES * 2];
    RzTexts();
    RzReadIni();
    __try {
        RzWeaponsInit(RzWindow(), &g_rzWt);
        for (stage = 0; stage < RZ_STAGES; ++stage)
            for (v = 0; v < 2; ++v) {
                const unsigned p = RzScript(stage, v);
                int k, dup = 0, m;
                for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
                if (dup) continue;
                seen[scripts++] = p;
                RzScanScript(RzWindow(), p, &g_rzScan[0]);
                total += g_rzScan[0].nPlain;
                wtotal += g_rzScan[0].nWeapon;
                m = RzStockMatch(stage, v, p, &g_rzScan[0]);
                if (m == 0 || (m < 0 && (g_rzScan[0].n || g_rzScan[0].bad))) ++g_rzSelfBad;
                RzWeaponsAdd(&g_rzWt, &g_rzScan[0]);
            }
        g_rzSelfN = total, g_rzSelfW = wtotal;
        g_rzWtOn = 1;
        g_rzOwnedOk = !memcmp((const void *)0x4F69C0, kObtained, sizeof kObtained);
        g_rzTakenOk = !memcmp((const void *)0x4F3B90, kTakenBits, sizeof kTakenBits);  // v3.48 fix1 RZ-001
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rzSelfN = -1;
        g_rzFail = 1;
        g_rzWtOn = 0;
    }
    for (i = 0x10; i <= 0x35; ++i) wn += g_rzWtOn && (RzWeaponFits(&g_rzWt, i, 0) || RzWeaponFits(&g_rzWt, i, 1));
    g_rzScanStage = -1;  // (the scans above were only the check)
    Log("randomizer: [Randomizer] Items %s, KeepSpecials %s, Room302 %s, PerVisit %s, Seed %lu; self-check: %d plain pickup(s) "
        "and %d weapon spot(s) in %d script(s) (stock %d / %d)%s; %d weapons may move%s; applied at each room load through "
        "004F0AF8 (patch-and-restore of the item and record words, no exe patch)",
        kRzItemsName[g_rzItems], kRzKeepName[g_rzKeep ? 1 : 0], kRzR302Name[g_rzR302 ? 1 : 0], kRzKeepName[g_rzPerVisit ? 1 : 0],
        (unsigned long)(unsigned)g_rzSeed, g_rzSelfN, g_rzSelfW, scripts, RZ_STOCK_TOTAL, RZ_STOCK_WEAPON,
        g_rzFail ? ": the scan FAULTED, the Randomizer is off" :
        g_rzSelfBad ? ": some scripts are not the stock ones (their pickups are left alone)" : ": the stock scripts", wn,
        g_rzOwnedOk ? "" : " (ItemIsObtained 004F69C0 is not the stock code: Random may draw weapons you own; they do not show)");
}
static void RzSessionLine(void)  // loghealth.inc LhSessionEndC (the session summary)
{
    Log("randomizer: session: Items %s (seed %lu, Keep Specials %s, Room 302 %s, Change Each Visit %s): %d room load(s), %d "
        "with pickups in the pool, %d pickup(s) changed, %d weapon(s) placed, %d plan(s), %d word(s) left as found%s",
        kRzItemsName[g_rzItems >= 0 && g_rzItems < RZI_N ? g_rzItems : 0], (unsigned long)(unsigned)g_rzSeed,
        kRzKeepName[g_rzKeep ? 1 : 0], kRzR302Name[g_rzR302 ? 1 : 0], kRzKeepName[g_rzPerVisit ? 1 : 0], g_rzLoads, g_rzRoomsPooled,
        g_rzChanged, g_rzWeapons, g_rzPlans, g_rzLeft, g_rzFail ? "; FAULTED (off)" : !g_enmInstalled ? "; no room-load hook (off)" : "");
}
