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
// v3.56 RI356 (the user's QA of v3.55, improvements 11, 12, 13, 16, 17, 18; staging/v356_feedback/w2_rando_items/PLAN.md):
//   - Specials Off / On (was Keep Specials, turned round: Off = Candles, Medallions and Swords stay where the game puts
//     them, On = they are drawn too; key items never move); [Randomizer] Specials, v3.55's KeepSpecials migrated once.
//   - Room 302 Items Off / Shuffle / Random, its own mode for stages 1 / 10 (was Out / In = drawn like Items); [Randomizer]
//     Room302 keeps its key, v3.55's In / Out migrated once (In -> the Items value in force).
//   - Reroll Visits By Room / By World ([Randomizer] Reroll; v3.55's PerVisit On -> By Room, Off -> By World, migrated
//     once). The visits are worldvisit.inc's (one rule with Enemies Return): a load is never a room or a world entry.
//     By Room: Random draws anew at each room entry; By World: once per world visit. Shuffle swaps a world's items: one
//     draw per world visit with both values.
//   - No Seed row and no New Seed row: the seed is drawn small under the page's description (RzSeedDraw, descfit.inc);
//     [Randomizer] Seed stays (missing or 0: rolled once and written).
//   - No "Next room." in the page's lines.
#include "rando_sim.h"
typedef char RzVisitFits[WV_ROOMS == RZ_ROOMS && WV_STAGES >= RZ_STAGES ? 1 : -1];  // v3.56 RI356: worldvisit.inc's counts
#include "rzmon_sim.h"  // v3.49 RZ349: the Monsters' pure part (rzmon.inc, included after enemies.inc / hitpool.inc)
// v3.49 RZ349L: the looks rows Eileen Outfit and Ghosts (rzlooks.inc, included after enemies.inc: it needs the outfit
// engine of eileenoutfit.inc and enemies.inc's spot test)
static void RzlTexts(void);
static void RzlReadIni(void);
static void RzlIniReread(void);
static void RzlSnapshot(void);
static int RzlChanged(void);
static void RzlDiscard(void);
static int RzlBuildRows(DWORD *t, int n);
static int RzlRowChanged(int row);
static void RzlGhostsBefore(int room, int d);
static void RzlOutfitWorld(void);
static void RzlOutfitRoom(void);
static void RzlSessionLine(void);

static int g_enmInstalled;  // enemies.inc (a tentative definition: the same object): its room-load hook 0x4F0AF8 is in
enum { RZS_ITEMS, RZS_SPECIALS, RZS_R302, RZS_SEED, RZS_REROLL, RZS_MON, RZS_N };  // the settings (g_rzSet, g_rzSnap)  // v3.49 RZ349: + RZS_MON  // v3.56 RI356: KEEP -> SPECIALS, PERVISIT -> REROLL
static volatile LONG g_rzItems = RZ_ITEMS_DEFAULT, g_rzSpecials = RZ_SPECIALS_DEFAULT, g_rzR302 = RZ_R302_DEFAULT;  // v3.56 RI356: Specials (was g_rzKeep), Room 302 0..2
static volatile LONG g_rzSeed;  // (an unsigned seed; never 0 once read)
static volatile LONG g_rzReroll = RZ_REROLL_DEFAULT;  // round 2: Change Each Visit  // v3.56 RI356: Reroll By Room / By World (was g_rzPerVisit)
// v3.56 RI356: the visit salt of a room's draw for the other randomizer rows (rzmon.inc, rzlooks.inc): By Room = the room's
// entries, By World = the world visit (worldvisit.inc; 0 = the seed's own)
#define RZ_PERVISIT_ON() (g_rzReroll == RZR_ROOM)
static unsigned RzVisitSalt(int stage, int room) { return g_rzReroll == RZR_ROOM ? WvRoomCount(stage, room) : WvWorld(); }
static volatile LONG g_rzMon = RM_MON_DEFAULT;  // v3.49 RZ349: [Randomizer] Monsters (rzmon.inc)
// v3.49 RZ349: the Monsters (rzmon.inc, included later): the row, the room load, rebuilt placements, start and end
static void RmRow(DWORD *r);
static const BYTE *RmText(int id);
static void __cdecl RmBeforeScript(int room);
static void __cdecl RmAfterScript(void);
static void RmDescFix(int room, int n, BYTE *desc);  // (enemies.inc EnmWalkC)
static void RmInit(void);
static void RmSessionLine(void);
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
// round 2: the visit counts (memory only, never saved)  // v3.56 RI356: now worldvisit.inc's (g_wv: room entries, world visits)
// the words written for the room being built (an item word and a record word a pickup at most)
static RzPatchStack g_rzStack;  // v3.48 fix1 RZ-002: one word list per (nested) room-script run (rando_sim.h)
static int g_rzTakenOk;         // v3.48 fix1 RZ-001: PickupTakenGetBits 0x4F3B90 is the stock code (gPickupTaken 0x1078618)
// the session's counts (the session summary)
static int g_rzLoads, g_rzRoomsPooled, g_rzChanged, g_rzWeapons, g_rzPlans, g_rzLeft, g_rzFirstSaid;
static volatile LONG g_rzQuotaAnom, g_rzQuotaRoom;

// ---------------------------------------------------------------- texts (the MSG_RZ0 block, 32 ids)
enum { RZM_T, RZM_R, RZM_DF, RZM_LI, RZM_VI0, RZM_DI0 = RZM_VI0 + RZI_N, RZM_LK = RZM_DI0 + RZI_N, RZM_VK0, RZM_DK0 = RZM_VK0 + 2,
       RZM_LR = RZM_DK0 + 2, RZM_VR0, RZM_DR0 = RZM_VR0 + RZI_N, RZM_NA = RZM_DR0 + RZI_N,
       RZM_LP, RZM_VP0, RZM_DP0 = RZM_VP0 + RZR_N, RZM_N = RZM_DP0 + RZR_N };  // round 2: + Change Each Visit's label and its two lines  // v3.56 RI356: Room 302 three values, Reroll's own two values; the Seed / New Seed texts gone (28 ids)
typedef char RzTextsFit[RZM_N <= 28 && RZM_N <= MSG_RZ1 - MSG_RZ0 ? 1 : -1];  // the MSG_RZ0 block (32 ids; 28 used)  // v3.49 MERGE: bounded by the next block MSG_RZ1 (was MSG_END) and below RZ349's RZ0 + 28 .. 29
static BYTE g_txtRz[RZM_N][64];
static DWORD g_rzIdsItems[RZI_N], g_rzIdsKeep[2], g_rzIdsR302[RZI_N], g_rzIdsReroll[RZR_N];  // v3.56 RI356: Room 302 3, + Reroll
static BYTE g_rzSeedTxt[40];  // v3.56 RI356: "Seed 1234567", drawn under the page's description (RzSeedDraw)
static const BYTE *RzText(int id)
{
    if ((id >= MSG_RZ0 + 28 && id < MSG_RZ0 + 30) || (id >= MSG_RZ1 && id < MSG_RZ1 + 16)) return RmText(id);  // v3.49 RZ349
    return id >= MSG_RZ0 && id < MSG_RZ0 + RZM_N ? g_txtRz[id - MSG_RZ0] : g_txtRz[RZM_NA];
}
static void RzSeedLabel(void)
{
    char s[40];
    sprintf_s(s, sizeof s, "Seed %lu", (unsigned long)(unsigned)g_rzSeed);
    Encode(g_rzSeedTxt, sizeof g_rzSeedTxt, s);  // v3.56 RI356: the small line (was the Seed row's label)
}
static void RzTexts(void)
{
    int k;
    // v3.53 OL1: the plan's words; every setting's line ends "Next room." (was "Next door.")
    // v3.56 RI356: no "Next room." on this page any more (the user, improvement 11): the plain lines (_VD / _D)
    Encode(g_txtRz[RZM_T], 64, OLW_T_RANDO);
    Encode(g_txtRz[RZM_R], 64, OLW_GAMEPLAY_RANDOMIZER_L);
    Encode(g_txtRz[RZM_DF], 64, OLW_GAMEPLAY_RANDOMIZER_D);  // v3.49 RZ349: + monsters
    Encode(g_txtRz[RZM_LI], 64, OLW_RANDO_ITEMS_L);
    for (k = 0; k < RZI_N; ++k) {
        Encode(g_txtRz[RZM_VI0 + k], 64, kRzItemsName[k]);
        g_rzIdsItems[k] = MSG_RZ0 + RZM_VI0 + k;
        Encode(g_txtRz[RZM_VR0 + k], 64, kRzR302Name[k]);  // v3.56 RI356: Off / Shuffle / Random (the ini words)
        g_rzIdsR302[k] = MSG_RZ0 + RZM_VR0 + k;
    }
    Encode(g_txtRz[RZM_DI0 + RZI_OFF], 64, OLW_RANDO_ITEMS_VD_OFF);
    Encode(g_txtRz[RZM_DI0 + RZI_SHUFFLE], 64, OLW_RANDO_ITEMS_VD_SHUFFLE);
    Encode(g_txtRz[RZM_DI0 + RZI_RANDOM], 64, OLW_RANDO_ITEMS_VD_RANDOM);
    Encode(g_txtRz[RZM_LK], 64, OLW_RANDO_SPECIALS_L);  // v3.56 RI356: Specials (was Keep Specials)
    for (k = 0; k < 2; ++k) {
        Encode(g_txtRz[RZM_VK0 + k], 64, kRzKeepName[k]);
        g_rzIdsKeep[k] = MSG_RZ0 + RZM_VK0 + k;
    }
    Encode(g_txtRz[RZM_DK0], 64, OLW_RANDO_SPECIALS_VD_OFF);
    Encode(g_txtRz[RZM_DK0 + 1], 64, OLW_RANDO_SPECIALS_VD_ON);
    Encode(g_txtRz[RZM_LR], 64, OLW_RANDO_ROOM_302_ITEMS_L);  // v3.53 OL1: Room 302 Items (was Room 302)
    Encode(g_txtRz[RZM_DR0 + RZI_OFF], 64, OLW_RANDO_ROOM_302_ITEMS_VD_OFF);  // v3.56 RI356: Off / Shuffle / Random
    Encode(g_txtRz[RZM_DR0 + RZI_SHUFFLE], 64, OLW_RANDO_ROOM_302_ITEMS_VD_SHUFFLE);
    Encode(g_txtRz[RZM_DR0 + RZI_RANDOM], 64, OLW_RANDO_ROOM_302_ITEMS_VD_RANDOM);
    RzSeedLabel();
    Encode(g_txtRz[RZM_NA], 64, OLW_NA_GAME);
    Encode(g_txtRz[RZM_LP], 64, OLW_RANDO_REROLL_L);  // v3.53 OL1: Reroll Visits (was Change Each Visit)  // v3.57 OLP1: Reroll
    for (k = 0; k < RZR_N; ++k) {  // v3.56 RI356: By Room / By World
        Encode(g_txtRz[RZM_VP0 + k], 64, kRzRerollName[k]);
        g_rzIdsReroll[k] = MSG_RZ0 + RZM_VP0 + k;
    }
    Encode(g_txtRz[RZM_DP0 + RZR_ROOM], 64, OLW_RANDO_REROLL_VD_BY_ROOM);  // v3.57 OLP1: the id (was _REROLL_VISITS_)
    Encode(g_txtRz[RZM_DP0 + RZR_WORLD], 64, OLW_RANDO_REROLL_VD_BY_WORLD);  // v3.49 RZ349: items and monsters  // v3.57 OLP1: the id
    RzlTexts();  // v3.49 RZ349L
}

// ---------------------------------------------------------------- [Randomizer]
static int RzAvail(void) { return g_enmInstalled && !g_rzFail; }
static volatile LONG *RzSetting(int which)
{
    switch (which) {
    case RZS_ITEMS: return &g_rzItems;
    case RZS_SPECIALS: return &g_rzSpecials;  // v3.56 RI356
    case RZS_R302: return &g_rzR302;
    case RZS_SEED: return &g_rzSeed;
    case RZS_MON: return &g_rzMon;  // v3.49 RZ349
    }
    return &g_rzReroll;  // v3.56 RI356 (was g_rzPerVisit)
}
static const char *const kRzKey[RZS_N] = { "Items", "Specials", "Room302", "Seed", "Reroll", "Enemies" };  // v3.49 RZ349: + Monsters  // v3.56 RI356: Specials, Reroll (were KeepSpecials, PerVisit)  // v3.56 RE356: Monsters -> Enemies (RzReadKeyMig reads the old key once)
static const char *RzSeedStr(unsigned s, char *b, size_t n)
{
    sprintf_s(b, n, "%lu", (unsigned long)s);
    return b;
}
static const char *RzValName(int which, LONG v, char *b, size_t n)
{
    if (which == RZS_ITEMS) return v >= 0 && v < RZI_N ? kRzItemsName[v] : "?";
    if (which == RZS_R302) return v >= 0 && v < RZI_N ? kRzR302Name[v] : "?";  // v3.56 RI356: Off / Shuffle / Random
    if (which == RZS_REROLL) return v >= 0 && v < RZR_N ? kRzRerollName[v] : "?";  // v3.56 RI356
    if (which == RZS_SEED) return RzSeedStr((unsigned)v, b, n);
    if (which == RZS_MON) return v >= 0 && v < RMM_N ? kRmMonName[v] : "?";  // v3.49 RZ349
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
// v3.56 RI356: a key of v3.55 read once into its new key (written; the old key removed); 1 = the old key was there
static int RzMigrate(const char *oldKey, const char *newKey, const char *oldText, const char *newText, const char *why)
{
    int ok;
    if (g_iniFallback) {
        Log("randomizer: [Randomizer] %s=%s (v3.55) is %s=%s now (%s): for this session (the ini cannot be written)", oldKey,
            oldText, newKey, newText, why);
        return 1;
    }
    ok = WritePrivateProfileStringA("Randomizer", newKey, newText, g_iniPath) != 0;
    if (ok && strcmp(oldKey, newKey)) WritePrivateProfileStringA("Randomizer", oldKey, NULL, g_iniPath);
    Log("randomizer: [Randomizer] %s=%s (v3.55) is %s=%s now (%s)%s", oldKey, oldText, newKey, newText, why,
        ok ? ": migrated once" : ": NOT written (applies for this session; read again at the next start)");
    return 1;
}
// v3.56 RI356: a new key missing from the file: its v3.55 key read once (-1 = neither: the new key's default is added)
static int RzReadOld(const char *newKey, const char *oldKey, int (*parse)(const char *), char *b, int n)
{
    if (g_iniFallback) return -1;
    GetPrivateProfileStringA("Randomizer", newKey, "\x01", b, (DWORD)n, g_iniPath);
    if (strcmp(b, "\x01")) return -1;  // the new key is there
    GetPrivateProfileStringA("Randomizer", oldKey, "\x01", b, (DWORD)n, g_iniPath);
    if (!strcmp(b, "\x01")) return -1;  // neither
    return parse(b) < 0 ? -2 : parse(b);
}
static int RzR302Now(const char *t)  // v3.56 RI356: Room302 read again (old values follow the Items value in force)
{
    int old;
    return RzR302Parse(t, (int)g_rzItems, &old);
}
// v3.56 RE356: a renamed key (rzmon_sim.h RzKeyPick): the new key's text; missing, the old key's, once: written as the new
// key, the old key removed, one line. Read raw: IniStr would add a missing new key with its default before the old one is
// read. With the settings file not writable (g_iniFallback): the values CreateIni found, nothing written.
static int RzReadKeyMig(const char *key, const char *oldKey, int (*parse)(const char *), int def, const char *const *names,
                        const char *what)
{
    char b[64], o[64];
    int v, how, w;
    o[0] = 0;
    if (g_iniFallback) {
        IniStr("Randomizer", key, b, sizeof b);
    } else {
        GetPrivateProfileStringA("Randomizer", key, "\x01", b, sizeof b, g_iniPath);
        if (!strcmp(b, "\x01")) b[0] = 0;
        if (!b[0]) GetPrivateProfileStringA("Randomizer", oldKey, "", o, sizeof o, g_iniPath);
    }
    how = RzKeyPick(b, o, parse, def, &v);
    if (how == RZK_BAD) Log("randomizer: [Randomizer] %s \"%s\" is not %s: %s (the default)", key, b, what, names[def]);
    if (how == RZK_NONE && !g_iniFallback && RzWrite(key, names[def]))
        Log("settings: [Randomizer] %s was missing, added as \"%s\"", key, names[def]);
    if (how == RZK_OLD || how == RZK_OLDBAD) {
        w = RzWrite(key, names[v]);
        if (w) WritePrivateProfileStringA("Randomizer", oldKey, NULL, g_iniPath);
        Log("randomizer: [Randomizer] %s \"%s\" is now %s %s (v3.56: the row was renamed%s; %s)", oldKey, o, key, names[v],
            how == RZK_OLDBAD ? "; not one of its values: the default" : "", w ? "written, the old key removed" :
            "NOT written: the old key is read again at the next start");
    }
    return v;
}
static void RzReadIni(void)
{
    char b[64], s[16];
    unsigned seed;
    int v, old;
    g_rzItems = RzReadKey("Items", RzItemsOfText, RZ_ITEMS_DEFAULT, kRzItemsName, "Off / Shuffle / Random");
    // v3.56 RI356: Specials (v3.55's KeepSpecials, turned round), Room302 Off / Shuffle / Random (v3.55's In / Out), Reroll
    // (v3.55's PerVisit): each old value read once and written in the new word
    v = RzReadOld("Specials", "KeepSpecials", RzOnOffOfText, b, sizeof b);
    if (v >= 0) {
        g_rzSpecials = RzSpecialsOfKeep(v);
        RzMigrate("KeepSpecials", "Specials", b, kRzKeepName[g_rzSpecials], "turned round: Specials On = they are drawn too");
    } else {
        if (v == -2) Log("randomizer: [Randomizer] KeepSpecials \"%s\" (v3.55) is not On / Off: Specials %s (the default)", b,
                         kRzKeepName[RZ_SPECIALS_DEFAULT]);
        g_rzSpecials = RzReadKey("Specials", RzOnOffOfText, RZ_SPECIALS_DEFAULT, kRzKeepName, "Off / On");
    }
    IniStr("Randomizer", "Room302", b, sizeof b);
    v = RzR302Parse(b, (int)g_rzItems, &old);
    if (v < 0) {
        Log("randomizer: [Randomizer] Room302 \"%s\" is not Off / Shuffle / Random: %s (the default)", b, kRzR302Name[RZ_R302_DEFAULT]);
        v = RZ_R302_DEFAULT;
    } else if (old) {
        RzMigrate("Room302", "Room302", b, kRzR302Name[v], RzR302OfText(b) ? "In = drawn like Items" : "Out = Room 302 keeps its pickups");
    }
    g_rzR302 = v;
    v = RzReadOld("Reroll", "PerVisit", RzOnOffOfText, b, sizeof b);
    if (v >= 0) {
        g_rzReroll = RzRerollOfPerVisit(v);
        RzMigrate("PerVisit", "Reroll", b, kRzRerollName[g_rzReroll], v ? "On = every room visit" :
                  "Off = one fixed draw a seed, folded into By World: one draw a world visit");
    } else {
        if (v == -2) Log("randomizer: [Randomizer] PerVisit \"%s\" (v3.55) is not On / Off: Reroll %s (the default)", b,
                         kRzRerollName[RZ_REROLL_DEFAULT]);
        g_rzReroll = RzReadKey("Reroll", RzRerollOfText, RZ_REROLL_DEFAULT, kRzRerollName, "By Room / By World");
    }
    g_rzMon = RzReadKeyMig("Enemies", "Monsters", RmMonOfText, RM_MON_DEFAULT, kRmMonName, "Off / Same World / All Worlds");  // v3.49 RZ349  // v3.50 RM350: + All Worlds  // v3.56 RE356: the key Enemies (Monsters read once)
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
    RzlReadIni();  // v3.49 RZ349L: EileenOutfit, Ghosts
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
    static const int kWhich[5] = { RZS_ITEMS, RZS_SPECIALS, RZS_R302, RZS_REROLL, RZS_MON };  // v3.49 RZ349: + RZS_MON  // v3.56 RI356: Specials, Reroll
    int (*const parse[5])(const char *) = { RzItemsOfText, RzOnOffOfText, RzR302Now, RzRerollOfText, RmMonOfText };  // v3.56 RI356
    char b[64], s[16];
    int k, v;
    unsigned seed;
    RzlIniReread();  // v3.49 RZ349L (its own unsaved flag)
    if (g_iniFallback || g_rzUnsaved) return;
    for (k = 0; k < 5; ++k) {  // v3.49 RZ349: 4 -> 5
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
    (void)s;
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
    RzlSnapshot();  // v3.49 RZ349L
}
static int RzChanged(void)
{
    int k;
    for (k = 0; k < RZS_N; ++k)
        if (*RzSetting(k) != g_rzSnap[k]) return 1;
    return RzlChanged();  // v3.49 RZ349L (was 0)
}
static void RzDiscard(void)  // under GpLock (gameplay.inc GpDiscard): "No" at the exit puts the snapshot back
{
    int k;
    for (k = 0; k < RZS_N; ++k)
        if (*RzSetting(k) != g_rzSnap[k]) RzSetLocked(k, g_rzSnap[k], "Seed back to");
    RzlDiscard();  // v3.49 RZ349L
}

// ---------------------------------------------------------------- the page (OP_RANDO)
static int g_rzRow[RZS_N] = { -1, -1, -1, -1, -1, -1 }, g_rzRowsN;  // v3.49 RZ349: 6 entries  // v3.56 RI356: no New Seed row (g_rzRowNew gone); RZS_SEED has no row
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
// v3.56 RI356: RzInKeep = the Specials row, RzInPerVisit = the Reroll Visits row (the names layout.inc finds them by)
static int __cdecl RzInKeep(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_SPECIALS, 2); }
static int __cdecl RzInR302(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_R302, RZI_N); }  // v3.56 RI356: 3 values
static int __cdecl RzInPerVisit(BYTE *cfg, int count) { (void)cfg; (void)count; return RzStep(RZS_REROLL, RZR_N); }  // round 2  // v3.56 RI356: By Room / By World
// v3.56 RI356: RzInSeed / RzInNew are gone with their rows (the seed is drawn under the description; [Randomizer] Seed)
static int __cdecl RzGetItems(void) { return (int)g_rzItems >= 0 && g_rzItems < RZI_N ? (int)g_rzItems : 0; }
static int __cdecl RzGetKeep(void) { return g_rzSpecials ? 1 : 0; }  // v3.56 RI356: Specials
static int __cdecl RzGetR302(void) { return (int)g_rzR302 >= 0 && g_rzR302 < RZI_N ? (int)g_rzR302 : 0; }  // v3.56 RI356: 0..2
static int __cdecl RzGetPerVisit(void) { return g_rzReroll == RZR_WORLD ? RZR_WORLD : RZR_ROOM; }  // v3.56 RI356: Reroll
static int __cdecl RzDescItems(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DI0 + RzGetItems()); }
static int __cdecl RzDescKeep(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DK0 + RzGetKeep()); }
static int __cdecl RzDescR302(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DR0 + RzGetR302()); }
static int __cdecl RzDescPerVisit(void) { return MSG_RZ0 + (!RzAvail() ? RZM_NA : RZM_DP0 + RzGetPerVisit()); }
static int __cdecl RzDescFolder(void) { return MSG_RZ0 + RZM_DF; }
static int __cdecl RzFolderIn(BYTE *cfg, int count) { (void)cfg; (void)count; return GpOpen(LP_RANDO); }  // v3.54 OL34: the new id
// Option 5's folder row (gameplay.inc GpBuildPages, under Special) and the page: Items, Keep Specials, Room 302, Change
// Each Visit, (v3.49 MERGE: Monsters, Eileen Outfit, Ghosts,) Seed, New Seed, Back. bcRow = Option 2's Button Config record (value-less rows: the Seed shows its number in
// the label). v3.56 RI356: Items, Specials, Room 302 Items, Reroll Visits, Monsters, Eileen Outfit, Ghosts, Back (no Seed,
// no New Seed; layout.inc places them in the plan's order).
static void RzFolderRow(DWORD *r, const DWORD *bcRow) { GpFolderRow(r, bcRow, RzFolderIn, MSG_RZ0 + RZM_R, RzDescFolder); }
static void RzBuildPage(const DWORD *stockRows, const DWORD *bcRow, DWORD *t, int parentRow)
{
    int n = 0;
    g_rzRow[RZS_ITEMS] = n;
    FillRow(t + 7 * n++, RzInItems, RzGetItems, g_rzIdsItems, RZI_N, MSG_RZ0 + RZM_LI, RzDescItems);
    g_rzRow[RZS_SPECIALS] = n;  // v3.56 RI356
    FillRow(t + 7 * n++, RzInKeep, RzGetKeep, g_rzIdsKeep, 2, MSG_RZ0 + RZM_LK, RzDescKeep);
    g_rzRow[RZS_R302] = n;
    FillRow(t + 7 * n++, RzInR302, RzGetR302, g_rzIdsR302, RZI_N, MSG_RZ0 + RZM_LR, RzDescR302);  // v3.56 RI356: 3 values
    g_rzRow[RZS_REROLL] = n;  // round 2  // v3.56 RI356: By Room / By World
    FillRow(t + 7 * n++, RzInPerVisit, RzGetPerVisit, g_rzIdsReroll, RZR_N, MSG_RZ0 + RZM_LP, RzDescPerVisit);
    g_rzRow[RZS_MON] = n;  // v3.49 RZ349: Monsters (rzmon.inc), under the Items' rows and Change Each Visit
    RmRow(t + 7 * n++);
    n = RzlBuildRows(t, n);  // v3.49 RZ349L: Eileen Outfit, Ghosts (v3.49 MERGE: moved here, right after w3's Monsters row)
    g_rzRow[RZS_SEED] = -1;  // v3.56 RI356: no Seed / New Seed rows
    (void)bcRow;
    g_rzRowsN = n;
    GpBackRow(t, n, stockRows);
    g_opTable[OP_RANDO] = t;
    g_opCount[OP_RANDO] = n + 1;
    g_opTitle[OP_RANDO] = MSG_RZ0 + RZM_T;
    g_opParent[OP_RANDO] = 4;
    g_opParentRow[OP_RANDO] = parentRow;
}
static int RzRowChanged(int row)  // the yellow mark (gameplay.inc GpRowChanged, OP_RANDO)
{
    int k;
    if (!g_snapOn || row < 0) return 0;
    for (k = 0; k < RZS_N; ++k)
        if (row == g_rzRow[k]) return *RzSetting(k) != g_rzSnap[k];
    return RzlRowChanged(row);  // v3.49 RZ349L (was 0)
}

// ---------------------------------------------------------------- v3.56 RI356: the seed under the description
// The user (improvements 17, 18): the seed takes no row; it is drawn in a smaller font under the Randomizer page's
// description line, only on that page and only while a randomizer row is on. descfit.inc DescText calls RzSeedDraw right
// after the description's text (its bottom and x); the line is font 1 at RZ_SEED_SCALE through subtitles.inc's glyph-pass
// range (SubSmallText: the same D hooks that size the subtitles; without them it is drawn at full size). Its glyphs are in
// the queue count LkGlyphNote measures right after, and in look.inc's page estimate (RzSeedGlyphs).
#define RZ_SEED_SCALE 0.72f
#define RZ_SEED_COL 0x606060  // a dim grey (0x80 = full)
static int RzlAnyOn(void);                                            // rzlooks.inc: Eileen Outfit / Ghosts on
static int SubSmallText(int n0, int n1, float x, float y, float s);  // subtitles.inc (v3.56 RI356)
static int RzAnyOn(void)
{
    return g_rzItems != RZI_OFF || g_rzR302 != RZI_OFF || g_rzMon != RMM_OFF || RzlAnyOn();
}
static int RzSeedShown(int lp) { return lp == LP_RANDO && RzAvail() && RzAnyOn() && g_rzSeedTxt[0] == 0x00 && g_rzSeedTxt[1] == 0x80; }
static int RzSeedGlyphs(int lp)  // the characters of the line (each drawn twice: shadow and text), 0 = not shown
{
    int i, n = 0;
    if (!RzSeedShown(lp)) return 0;
    for (i = 2; i + 1 < (int)sizeof g_rzSeedTxt && !(g_rzSeedTxt[i] == 0xFF && g_rzSeedTxt[i + 1] == 0xFF); ++i) ++n;
    return n;
}
static void RzSeedDraw(int x, float bottom)
{
    typedef int(__cdecl *RzTextFn)(int, int, int, int, int, int);
    const DWORD c00 = *(volatile DWORD *)0x1261C00, c14 = *(volatile DWORD *)0x1261C14, c18 = *(volatile DWORD *)0x1261C18;
    const int glyphs = RzSeedGlyphs(g_lpage);
    const int y = (int)(bottom + 2.0f);
    int n0, n1;
    static int s_said;
    if (!glyphs) return;
    n0 = *(volatile int *)0x1261BF4;
    if (n0 < 0 || n0 + 2 * glyphs > 790) return;  // the text queue holds 800: the line never takes the last places
    ((Fn1)0x556A40)(1);
    ((Fn1)0x556A50)(8);
    ((RzTextFn)0x50B5B0)(-1, (int)(ULONG_PTR)g_rzSeedTxt, x + 2, y + 2, 0, 0);
    ((Fn1)0x556A90)(RZ_SEED_COL);
    ((RzTextFn)0x50B5B0)(-1, (int)(ULONG_PTR)g_rzSeedTxt, x, y, 0, 0);
    n1 = *(volatile int *)0x1261BF4;
    {
        const int small = SubSmallText(n0, n1, (float)x, (float)y, RZ_SEED_SCALE);
        if (!s_said++)
            Log("randomizer: the seed %lu is drawn under the page's description (x %d, y %d, %d glyph(s), %s)",
                (unsigned long)(unsigned)g_rzSeed, x, y, n1 - n0, small ? "the smaller size" :
                "full size: the subtitle hooks are not in");
    }
    *(volatile DWORD *)0x1261C00 = c00;
    *(volatile DWORD *)0x1261C14 = c14;
    *(volatile DWORD *)0x1261C18 = c18;
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
static void RzPlanLog(int stage, int v, int mode, int reroll)  // v3.56 RI356: reroll = RZR_* (was perVisit)
{
    const RzPlanStat *s = &g_rzStat[v];
    char sp[200];
    int o = 0, k;
    static const int kSp[3] = { RZ_ITEM_CANDLE, RZ_ITEM_MEDALLION, RZ_ITEM_SWORD };
    if (!s->pooled) return;
    for (k = 0; k < 3; ++k)
        o += sprintf_s(sp + o, sizeof sp - o, "%s%s %d (stock %d)", k ? ", " : "", RzItemName(kSp[k]), s->cnt[kSp[k]], s->stock[kSp[k]]);
    // v3.56 RI356: Specials / Room 302 Items / Reroll (were Keep Specials / Room 302 / Change Each Visit), the world visit
    // worldvisit.inc's (0 = the seed's own)
    Log("randomizer: stage %d visit %d: %s, %d of %d pickup(s) in the pool (%d weapon spot(s)), %d changed (seed %lu, "
        "Specials %s, Room 302 Items %s, Reroll %s, world visit %u); %s, weapons %d (stock %d)%s%s", stage, v,
        kRzItemsName[mode], s->pooled, g_rzScan[v].n, g_rzScan[v].nWeapon, s->changed, (unsigned long)(unsigned)g_rzSeed,
        kRzKeepName[g_rzSpecials ? 1 : 0], kRzR302Name[RzGetR302()], kRzRerollName[reroll == RZR_WORLD ? 1 : 0], WvWorld(),
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
// v3.56 RI356: Shuffle = one draw a world visit (worldvisit.inc); Random By Room = the room's entries, By World = the world
// visit for every room; mode = the stage's (Room 302 has its own row); never the fixed v1 sharing of both halves
static void RzPlanNow(int stage, int mode)
{
    const int keep = !g_rzSpecials, r302 = 1, reroll = g_rzReroll == RZR_WORLD ? RZR_WORLD : RZR_ROOM;
    const unsigned seed = (unsigned)g_rzSeed;
    RzPlanIn in;
    unsigned key;
    int v;
    if (stage != g_rzScanStage) RzScanStage(stage);
    in.mode = mode, in.keep = keep, in.r302 = r302, in.share = 0, in.seed = seed;
    in.worldSalt = WvWorld();
    in.roomSalt = reroll == RZR_ROOM && stage >= 0 && stage < WV_STAGES ? g_wvVis.room[stage] : NULL;  // v3.56 MERGE: part 2 of the one rule
    in.roomAll = reroll == RZR_WORLD ? WvWorld() : 0;
    in.owned = mode == RZI_RANDOM ? RzOwned() : 0;
    in.taken = g_rzTakenOk && stage >= 0 && stage < RZ_STAGES ?  // v3.48 fix1 RZ-001: gPickupTaken[stage] (50 rooms x 8 bytes)
               (const unsigned char *)(ULONG_PTR)(0x1078618u + (unsigned)stage * (RZ_TAKEN_ROOMS * RZ_TAKEN_SLOTS / 8)) : NULL;
    in.wt = g_rzWtOn ? &g_rzWt : NULL;
    RzPlanStage(&in, stage, &g_rzScan[0], g_rzScanHas1 ? &g_rzScan[1] : NULL, g_rzOut[0], g_rzPool[0], g_rzRec[0], &g_rzStat[0],
                g_rzOut[1], g_rzPool[1], g_rzRec[1], &g_rzStat[1]);
    ++g_rzPlans;
    // one plan line per world visit / settings change (not per room: with Change Each Visit Random plans every room load)
    key = RzHash4(seed, (unsigned)stage, (unsigned)mode | (unsigned)keep << 4 | (unsigned)r302 << 5 | (unsigned)reroll << 6,
                  in.worldSalt ^ 0x504C414Eu);
    if (g_rzPlanSaid && key == g_rzPlanKey) return;
    g_rzPlanKey = key, g_rzPlanSaid = 1;
    for (v = 0; v < 1 + g_rzScanHas1; ++v)
        if (g_rzVisitOk[v]) RzPlanLog(stage, v, mode, reroll);
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
    const int stage = (int)*(volatile DWORD *)0xFD5A90, mode = RzStageMode(stage, (int)g_rzItems, (int)g_rzR302);  // v3.56 RI356: Room 302's own mode
    unsigned start, rk = 0;
    int visit, i, pooled = 0, changed = 0, other = 0, weapons = 0;
    char list[700];
    int o = 0;
    ++g_rzLoads;
    if (!g_rzFirstSaid++)
        Log("randomizer: the first room load through 004F0AF8 (stage %d room %d): Items %s", stage, room,
            mode >= 0 && mode < RZI_N ? kRzItemsName[mode] : "?");
    if (stage < 0 || stage >= RZ_STAGES || room <= 0 || room >= RZ_ROOMS) return;
    rk = WvRoomCount(stage, room);  // v3.56 RI356: worldvisit.inc counted this room load (EnmWrap, first): never a load
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
        RmBeforeScript(room);  // v3.49 RZ349: (its level too: RmAfterScript pops it; nothing written that deep)
        return d;
    }
    if (d > 0 && LogQuota(&g_rzQuotaAnom, 20))
        LogAnomaly("randomizer", "room %d: a room build inside another room build (level %d): its own word list", room, d + 1);
    if (g_rzFail) {
        RmBeforeScript(room);  // v3.49 RZ349: the monsters' words (rzmon.inc: their own list, their own fault flag)
        return d;
    }
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
    RmBeforeScript(room);  // v3.49 RZ349: after the Items (the room's visit count is counted there)
    RzlGhostsBefore(room, d);  // v3.49 RZ349L: Ghosts Spots (rzlooks.inc; its own __try, this run's level d)
    return d;
}
static void __cdecl RzAfterScript(int d)
{
    int other = 0;
    RmAfterScript();  // v3.49 RZ349: the monsters' words of this run first (pushed last)
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
    Log("randomizer: [Randomizer] Items %s, Specials %s, Room302 %s, Reroll %s, Seed %lu; self-check: %d plain pickup(s) "
        "and %d weapon spot(s) in %d script(s) (stock %d / %d)%s; %d weapons may move%s; applied at each room load through "
        "004F0AF8 (patch-and-restore of the item and record words, no exe patch)",
        kRzItemsName[g_rzItems], kRzKeepName[g_rzSpecials ? 1 : 0], kRzR302Name[RzGetR302()], kRzRerollName[RzGetPerVisit()],
        (unsigned long)(unsigned)g_rzSeed, g_rzSelfN, g_rzSelfW, scripts, RZ_STOCK_TOTAL, RZ_STOCK_WEAPON,
        g_rzFail ? ": the scan FAULTED, the Randomizer is off" :
        g_rzSelfBad ? ": some scripts are not the stock ones (their pickups are left alone)" : ": the stock scripts", wn,
        g_rzOwnedOk ? "" : " (ItemIsObtained 004F69C0 is not the stock code: Random may draw weapons you own; they do not show)");
    RmInit();  // v3.49 RZ349: the Monsters' texts, self-check and save filter
}
static void RzSessionLine(void)  // loghealth.inc LhSessionEndC (the session summary)
{
    WvSessionLine();  // v3.56 RI356: the world-visit rule's counts
    Log("randomizer: session: Items %s (seed %lu, Specials %s, Room 302 Items %s, Reroll %s): %d room load(s), %d "
        "with pickups in the pool, %d pickup(s) changed, %d weapon(s) placed, %d plan(s), %d word(s) left as found%s",
        kRzItemsName[g_rzItems >= 0 && g_rzItems < RZI_N ? g_rzItems : 0], (unsigned long)(unsigned)g_rzSeed,
        kRzKeepName[g_rzSpecials ? 1 : 0], kRzR302Name[RzGetR302()], kRzRerollName[RzGetPerVisit()], g_rzLoads, g_rzRoomsPooled,
        g_rzChanged, g_rzWeapons, g_rzPlans, g_rzLeft, g_rzFail ? "; FAULTED (off)" : !g_enmInstalled ? "; no room-load hook (off)" : "");
    RmSessionLine();  // v3.49 RZ349
    RzlSessionLine();  // v3.49 RZ349L
}
