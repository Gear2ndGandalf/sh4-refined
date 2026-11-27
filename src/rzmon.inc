// ================================================================ v3.49 RZ349: the Randomizer's Monsters (Option 5 > Randomizer)
// Plan: staging/v349_feedback/w3_monsters/PLAN.md (research: staging/research_randomizer/REPORT.md sections 1, 2, 6, 7; the
// user's answers: a mix per enemy, any strength). Included by sh4windowmod.c after hitpool.inc / recevict.inc (it uses
// enemies.inc's file loader, spot test and records, hitpool.inc's pool size); randomizer.inc owns the setting
// ([Randomizer] Monsters, RZS_MON), declares the entry points and calls RmBeforeScript / RmAfterScript from its
// RzBeforeScript / RzAfterScript (enemies.inc EnmWrap: right around the stock room script runner 0x52CA00, the restore
// in the __finally).
//
// At a room load of stages 2..8 every source placement of the room's script (rzmon_sim.h RmIsSource: Whitestool, Hummer,
// Gum Head, Twin Victim, Tremer, Sniffer Dog in spawn mode 0, upright, no id override, no story flag; never a story,
// boss or event spawn, never a ghost, Walter, mm, Wall Man, Wheelchair or the Patient) rolls its own type (seeded like
// Items: the seed, the stage and half, the room, the object number and, with Change Each Visit, the room's visit count):
//   This Room:  among the targets whose files are resident in this room now (EnmLoadedHere);
//   This World: also among the targets this world places (both halves' scripts, any mode), their missing files loaded at this room
//               load by Enemies Follow's loader (EnmEnsureFiles: 16 extra files a room, 30 MB of the arena kept free).
// Targets: Whitestool, Hummer, Twin Victim (2 variants), Tremer, Sniffer Dog (2), and Gum Head (2) only while Playable
// Eileen's crash guard of the Gum Head's group call is in (0x49B70A and 0x45C3C0 in its SH4PE_Patches; else out).
// The placement's command-7 type word, its mode word (another type: spawn mode 0, upright, its variant) and, for a walker
// at a Hummer's spot, its y (the floor under it, EnmSpotTest) are written before the runner and restored right after (the
// patch stack, as the Items' words). Budgets: bones (the room never needs more than max(the pool's free part, the stock
// room's estimate)), tasks / actors / the enemy list (EnmRoomForOne, else the room stays as placed), the hit-shape pool
// (the stock 100 shapes: Monsters stays off), files (above). A monster made as another type writes a record of that type
// when its room is left: while that record lives the placement is made as that type again (the type's restore reads it);
// a record whose type cannot be made here is deleted (that monster comes back fresh); with Change Each Visit On a record
// in this room keeps no type: the room's monsters roll again (one rolled to another type comes back fresh, at its
// placement). The save's copy of the records
// (0x465468, the `call 42BBB0` of EnemyManagerSave) is cleaned of every such record: a save never holds a randomized
// monster (it loads as the stock game would place it). Rebuilt placements (Enemy Copies, Follow, the wanderers) take the
// type they live as (RmDescFix, called by enemies.inc EnmWalkC). With the cheat table's Enemy Randomizer (entry 7700:
// 0x52CAA9 / 0x52CB2B not stock) Refined's Monsters stay off (logged at each change).
// v3.50 RM350 (staging/v350_feedback/w2_monsters/PLAN.md): a save-based game rolls too: the game's own record of a stock
// monster in its room (every room a save visited holds them) is adopted (rolled; deleted when the roll gives another type)
// instead of keeping the room stock (the user's v3.49 QA: 1 of 15 placements changed); All Worlds (any target in any world,
// files as This World); the room line counts why each placement kept its stock type.
// v3.51 SN351 (staging/v351_feedback/w4_sniffers/PLAN.md; the user's v3.50 QA item 4): the Subway's first Sniffer Dogs (room
// 2:2's event dogs, spawn mode 7 / 6 / 6) roll too once story flag 0x130 is set (their cut-scene is over), else they stay as
// placed; a roll that gives the placement's own type rolls once more (hidden [Randomizer] MonsterRollAgain=0: v3.50's roll).
#include "rzmon_sim.h"

static int g_rmFail;                 // faulted: off for this session
static int g_rmSelfN = -1, g_rmSelfK, g_rmSelfBad, g_rmScripts;  // the start's scan: sources / kind-2 / non-stock scripts
static RmScan g_rmScan[2];           // the stage in hand, both halves' scripts
static int g_rmScanStage = -1, g_rmScanHas1, g_rmVisitOk[2], g_rmBadSaid[RZ_STAGES][2];
static unsigned g_rmTypes;           // bit t: type t is placed by this stage's scripts (the silent-type note)
static unsigned char g_rmCurT[2][RM_VMAX], g_rmCurV[2][RM_VMAX];  // the ledger: what each source was last made as here
                                                                  // (this world visit; type 0 = its stock type)
static RmPatchStack g_rmStack;       // the words of each (nested) room-script run
static int g_rmGum = -1;             // Gum Heads may be targets (Playable Eileen's guard; decided at the first room load)
static int g_rmCt = -1;              // the cheat table's Enemy Randomizer was on at the last room load (-1 = not seen)
static int g_rmHitSaid, g_rmSaveSite, g_rmSaveIni = 1, g_rmFirstSaid;
static unsigned g_rmSilent[RZ_STAGES];  // the types already logged as "not of this world" (per stage)
static int g_rmLoads, g_rmRooms, g_rmChanged, g_rmSticky, g_rmDrops, g_rmFileTypes, g_rmFileKb, g_rmKeptBones, g_rmKeptFiles,
           g_rmKeptFloor, g_rmLeft, g_rmSaves, g_rmSaveCleaned, g_rmOffDrops, g_rmCtLoads;
static volatile LONG g_rmQuotaRoom, g_rmQuotaAnom, g_rmQuotaFiles;
static RmProv g_rmProv;              // v3.49 fix2 RZ349-SAVE-001: what Refined made each placement as (rzmon_sim.h RmProv)
static int g_rmOther;                // v3.49 fix2: placements left as placed for a record Refined did not make (another writer)
static volatile LONG g_rmQuotaOther;
static int g_rmAdopted, g_rmAdoptChanged, g_rmRollStock, g_rmSame, g_rmAway, g_rmNone;  // v3.50 RM350: the reasons, the session
static int g_rmRollAgain = 1;        // v3.51 SN351: [Randomizer] MonsterRollAgain (hidden, ini only; 0 = v3.50's roll)
static int g_rmSelfEv = -1;          // v3.51 SN351: event dogs among the start's sources
static int g_rmAgain, g_rmEvPlanned, g_rmEvAsleep;  // v3.51 SN351: the session: rolled again, event dogs planned / kept asleep
static unsigned g_rmStorySaid[RZ_STAGES];  // v3.56 RE356: the story rule's line said (bit: which flags were set)

// ---------------------------------------------------------------- texts (MSG_RZ0 + 28 .. + 29, MSG_RZ1 + 0 .. 15)
// v3.50 RM350: All Worlds' value and description appended (RZ1 + 5 / + 6: the v3.49 ids stay as they were)
// v3.56 RE356: the row Enemies, three values Off / Same World / All Worlds (row index = kRmRowMode's): the label, Same World,
// the descriptions of Off and Same World, the cheat table's line, All Worlds and its description (the same ids, fewer used)
enum { RMT_LABEL, RMT_WORLD, RMT_D0, RMT_DWORLD, RMT_CT, RMT_ALL, RMT_DALL, RMT_N };
static int RmDescText(int mode) { return mode == RMM_ALL ? RMT_DALL : mode == RMM_OFF ? RMT_D0 : RMT_DWORLD; }  // v3.50 RM350  // v3.56 RE356
static BYTE g_txtRm[RMT_N][64];
static DWORD g_rmIdsMon[RM_ROW_N];  // v3.56 RE356: by row index (was by mode)
static int RmTextId(int k) { return k < 2 ? MSG_RZ0 + 28 + k : MSG_RZ1 + k - 2; }
typedef char RmTextsFit[RMT_N - 2 <= 16 && MSG_RZ0 + 30 <= MSG_RZ1 && MSG_RZ1 + 16 <= MSG_V50 ? 1 : -1];  // v3.50 MERGE: MSG_V50 (was MSG_END)
static const BYTE *RmText(int id)
{
    const int k = id >= MSG_RZ0 + 28 && id < MSG_RZ0 + 30 ? id - (MSG_RZ0 + 28) : id >= MSG_RZ1 && id < MSG_RZ1 + 16 ? id - MSG_RZ1 + 2 : -1;
    return k >= 0 && k < RMT_N ? g_txtRm[k] : g_txtRz[RZM_NA];
}
static void RmTexts(void)
{
    // v3.56 RE356: the row Enemies (was Monsters); its words without the "Next room." tag (the user's improvement 11)
    Encode(g_txtRm[RMT_LABEL], 64, OLW_RANDO_ENEMIES_L);
    Encode(g_txtRm[RMT_WORLD], 64, OLW_RANDO_ENEMIES_V_SAME_WORLD);
    Encode(g_txtRm[RMT_D0], 64, OLW_RANDO_ENEMIES_VD_OFF);
    Encode(g_txtRm[RMT_DWORLD], 64, OLW_RANDO_ENEMIES_VD_SAME_WORLD);
    Encode(g_txtRm[RMT_CT], 64, OLW_RANDO_CT);
    Encode(g_txtRm[RMT_ALL], 64, OLW_RANDO_ENEMIES_V_ALL_WORLDS);
    Encode(g_txtRm[RMT_DALL], 64, OLW_RANDO_ENEMIES_VD_ALL_WORLDS);
    g_rmIdsMon[0] = MSG_RZ0 + RZM_VK0;  // randomizer.inc's "Off"
    g_rmIdsMon[1] = (DWORD)RmTextId(RMT_WORLD);
    g_rmIdsMon[2] = (DWORD)RmTextId(RMT_ALL);
}

// ---------------------------------------------------------------- the row (randomizer.inc RzBuildPage)
static int RmAvail(void) { return g_enmInstalled && !g_rmFail && g_rmSelfN >= 0; }
static int RmCtOn(void)  // the cheat table's Enemy Randomizer (7700) has the runner's two create calls
{
    return memcmp((const void *)0x52CAA9, "\xE8\x52\xFE\xFF\xFF", 5) != 0 || memcmp((const void *)0x52CB2B, "\xE8\xD0\xFD\xFF\xFF", 5) != 0;
}
static int __cdecl RmInMon(BYTE *cfg, int count)
{
    int v;
    (void)cfg;
    (void)count;
    if (!RmAvail()) {
        if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue(RmRowOfMode((int)g_rzMon), RM_ROW_N);  // v3.56 RE356: by row index (Off / Same World / All Worlds)
    if (v >= 0 && v < RM_ROW_N) RzSetLocked(RZS_MON, kRmRowMode[v], "");
    GpUnlock();
    return 0;
}
static int RmMode(void) { return (int)g_rzMon >= 0 && g_rzMon < RMM_N ? (int)g_rzMon : 0; }  // v3.56 RE356
static int __cdecl RmGetMon(void) { return RmRowOfMode(RmMode()); }  // v3.56 RE356: the row index (was the mode)
static int __cdecl RmDescMon(void)
{
    if (!RmAvail()) return MSG_RZ0 + RZM_NA;
    if (g_rmCt > 0 && RmMode() != RMM_OFF) return RmTextId(RMT_CT);
    return RmTextId(RmDescText(RmMode()));  // v3.50 RM350 (All Worlds' description sits after the cheat table's)
}
static void RmRow(DWORD *r) { FillRow(r, RmInMon, RmGetMon, g_rmIdsMon, RM_ROW_N, RmTextId(RMT_LABEL), RmDescMon); }  // v3.56 RE356: 3 values

// ---------------------------------------------------------------- the scan of the stage in hand
static void RmScanStage(int stage)  // (no word of ours is in the scripts now: RmAfterScript restored them)
{
    const unsigned p0 = RzScript(stage, 0), p1 = RzScript(stage, 1);
    int v;
    g_rmScanStage = stage;
    g_rmScanHas1 = p1 != p0;
    g_rmTypes = 0;
    memset(g_rmCurT, 0, sizeof g_rmCurT), memset(g_rmCurV, 0, sizeof g_rmCurV);  // (a new world visit's ledger)
    for (v = 0; v < 1 + g_rmScanHas1; ++v) {
        int m;
        RmScanScript(RzWindow(), v ? p1 : p0, stage, &g_rmScan[v]);
        m = RmStockMatch(stage, v, v ? p1 : p0, &g_rmScan[v]);
        g_rmVisitOk[v] = m == 1 || (m < 0 && !g_rmScan[v].n && !g_rmScan[v].bad);
        g_rmTypes |= g_rmScan[v].types;
        if (!g_rmVisitOk[v] && !g_rmBadSaid[stage][v]++)
            LogAnomaly("randomizer", "monsters: stage %d visit %d: the script at %08X is not the stock one (%d source placement(s) of "
                       "%d%s): its monsters are left alone", stage, v, v ? p1 : p0, g_rmScan[v].n, g_rmScan[v].kind2,
                       g_rmScan[v].bad ? ", unreadable" : g_rmScan[v].overflow ? ", too many" : "");
    }
    if (!g_rmScanHas1) g_rmVisitOk[1] = 0, memset(&g_rmScan[1], 0, sizeof g_rmScan[1]);
}
static int RmVisOf(int room, unsigned start)  // which half's scan the room's script start belongs to (-1 = neither)
{
    if (room <= 0 || room >= RZ_ROOMS || !start) return -1;
    if (g_rmScan[0].roomStart[room] == start) return 0;
    if (g_rmScanHas1 && g_rmScan[1].roomStart[room] == start) return 1;
    return -1;
}

// ---------------------------------------------------------------- Playable Eileen's Gum Head guard
static int RmPeHook(DWORD va)  // a jmp into Playable Eileen's dsound.dll, listed in its SH4PE_Patches
{
    const BYTE *p = (const BYTE *)(ULONG_PTR)va;
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    const CamPeList *L;
    DWORD lo = 0, hi = 0, to, i, n;
    if (!ds || p[0] != 0xE9 || !HasExport(ds, "SH4PE_Patches")) return 0;
    CamModRange(ds, &lo, &hi);
    to = va + 5 + *(const DWORD *)(p + 1);
    if (to < lo || to >= hi) return 0;
    L = (const CamPeList *)GetProcAddress(ds, "SH4PE_Patches");
    if (!L || L->magic != 0x45504853) return 0;
    n = L->count > 512 ? 512 : L->count;
    for (i = 0; i < n; ++i)
        if (L->item[i].va <= va && va < L->item[i].va + L->item[i].len) return 1;
    return 0;
}
// Gum Heads as targets: the stock game crashes at 0x45C3C4 when a Gum Head takes a group-call token whose poster is gone
// (0x49B70A stores the poster looked up by id without a NULL test; KB "Gum Head null-partner crash"). Playable Eileen
// v0.78+ guards both reads for every Gum Head (0x49B70A: no poster = the code's own "no token" path; 0x45C3C0: a NULL
// actor's position = the player's): with both hooks in, Gum Heads are in; else out. Decided once (dsound.dll patches in
// its DllMain, long before the first room load).
static int RmGumGuard(void)
{
    if (g_rmGum < 0) {
        int a = 0, b = 0;
        __try {
            a = RmPeHook(0x49B70A), b = RmPeHook(0x45C3C0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            a = b = 0;
        }
        g_rmGum = a && b;
        Log("randomizer: monsters: Gum Heads %s (Playable Eileen's crash guard of the Gum Head's group call: 0049B70A %s, "
            "0045C3C0 %s)", g_rmGum ? "may be rolled" : "are never rolled", a ? "in" : "not in", b ? "in" : "not in");
    }
    return g_rmGum;
}

// ---------------------------------------------------------------- the room load (randomizer.inc RzBeforeScript)
static BYTE *RmRecLive(unsigned id)  // the game's record of id (state != 0), NULL = none
{
    return (BYTE *)RmRecFind(ENM_RECORDS, RM_REC_N, id);
}
// v3.49 fix2 RZ349-SAVE-001: the provenance of a placement Refined just built as (type, variant): kept as it is while a
// record of Refined's lives in another room (a follower, a wanderer: that record stays Refined's)
static void RmProvAfterBuild(int stage, int half, int room, const RmSlot *s, int type, int variant)
{
    const unsigned id = EnmScriptId(stage, half, room, s->n);
    const BYTE *r = RmRecLive(id);
    if (r && RmProvOurs(&g_rmProv, r) && RmRecRoom(r) != room) return;
    RmProvPut(&g_rmProv, id, type, variant, s->type, s->variant);
}
// a room's records that Refined did not make and that hold another type than the stock one: left alone, logged (quota)
static void RmOtherSay(int stage, int room, int n)
{
    g_rmOther += n;
    if (n && LogQuota(&g_rmQuotaOther, 50))
        Log("randomizer: monsters: room %d:%d: %d record(s) of another type that Refined did not make (another writer): those "
            "placements stay as placed, the records are kept", stage, room, n);
}
// Monsters Off (or a room left as placed): a record in this room that Refined made as another type (an earlier randomized
// visit, still exactly that type) is deleted, so the stock create never restores Refined's record of another type; any
// other record is left alone (v3.49 fix2 RZ349-SAVE-001: provenance, rzmon_sim.h RmOffDrops)
static int RmDropForeign(int stage, int half, int room, const int *idx, int m, int vis)
{
    int k, n = 0, others = 0;
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &g_rmScan[vis].s[idx[k]];
        BYTE *r = RmRecLive(EnmScriptId(stage, half, room, s->n));
        if (RmOffDrops(&g_rmProv, r, stage, room)) {
            ENM_RECDEL(r);
            ++n;
        } else if (r && RmRecRoom(r) == room && !RmProvOurs(&g_rmProv, r) && !(RmRecType(r) == s->type && RmRecVariant(r) == s->variant)) {
            ++others;
        }
        RmProvAfterBuild(stage, half, room, s, s->type, s->variant);
        g_rmCurT[vis][idx[k]] = 0, g_rmCurV[vis][idx[k]] = 0;
    }
    RmOtherSay(stage, room, others);
    return n;
}
// v3.56 RE356: a placement's position words (command 8: x at yVa - 4, y at yVa, z at yVa + 4) set to want (float bits) on
// this run's level d; 1 = every differing word written (none = 1 too)
static int RmPosWords(int d, const RmSlot *s, const float *want)
{
    int a;
    for (a = 0; a < 3; ++a) {
        unsigned u0, u1;
        memcpy(&u0, &s->pos[a], 4), memcpy(&u1, &want[a], 4);
        if (u0 != u1 && !RmStackAdd(&g_rmStack, d, (unsigned *)(ULONG_PTR)(s->yVa + 4u * (unsigned)a - 4u), u0, u1)) return 0;
    }
    return 1;
}
// v3.56 RE356: Spawn Site with Enemies Off (or a room left as placed but a boss room): each source at its site as its stock
// type (spawnsite.inc SsMonSite), its words and its standing record here moved (SsMonRecMove)
static void RmSitesStock(int stage, int half, int room, int d, int vis, const int *idx, int m)
{
    int k, left = 0;
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &g_rmScan[vis].s[idx[k]];
        float site[3], hover = 0.0f;
        int keep, n0;
        if (RmIsFlyer(s->type)) {  // a Hummer keeps its own height above its floor
            float base[3], cand[3];
            memcpy(base, s->pos, 12), memcpy(cand, s->pos, 12);
            if (EnmFinite3(base) && EnmSpotTest(base, cand, 0, 0.0f) == ENM_SPOT_OK && _finite(cand[1])) hover = cand[1] - s->pos[1];
        }
        {
            const BYTE *r = RmRecLive(EnmScriptId(stage, half, room, s->n));  // (recorded in another room: not made here)
            if (!SsMonSite(stage, half, room, s, s->type, 0, ENM_DOGS_AWAKE, r && RmRecStage(r) == stage && RmRecRoom(r) != room, hover,
                           site, &keep))
                continue;
        }
        n0 = d >= 0 && d < RZ_DEPTH ? g_rmStack.n[d] : 0;
        if (!RmPosWords(d, s, site)) {
            RmStackTrim(&g_rmStack, d, n0);
            ++left;
            --g_ssRmMoved, ++g_ssRmKept;  // (its site's count back)
            continue;
        }
        SsMonRecMove(EnmScriptId(stage, half, room, s->n), stage, room, s->type, s->variant, site);
    }
    g_rmLeft += left;
    if (left && LogQuota(&g_rmQuotaAnom, 20))
        LogAnomaly("randomizer", "spawn site: room %d:%d: %d placement word(s) did not hold the stock value (another mod?): left as found",
                   stage, room, left);
    SsMonRoomSay(stage, room);
}
static void RmBeforeScriptC(int room, int d)
{
    const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0, mode = (int)g_rzMon;
    int idx[RM_ROOM_MAX], m = 0, vis, i, k, ct, why = 0, kb = 0, loaded = 0, other = 0, drops;
    unsigned start;
    RmRec rec[RM_ROOM_MAX];
    unsigned char floorOk[RM_ROOM_MAX];
    float floorY[RM_ROOM_MAX];
    RmPick pick[RM_ROOM_MAX];
    RmPlanStat st;
    RmPlanIn in;
    float site[RM_ROOM_MAX][3];          // v3.56 RE356: Spawn Site
    unsigned char sited[RM_ROOM_MAX];    // v3.56 RE356
    char list[900];
    int o = 0;
    ++g_rmLoads;
    memset(sited, 0, sizeof sited);  // v3.56 RE356
    if (stage < 2 || stage > 8 || room <= 0 || room >= RZ_ROOMS) return;  // (only the monster worlds)
    if (stage != g_rmScanStage) RmScanStage(stage);
    start = ((const DWORD *)0x10A0A78)[room];
    vis = RmVisOf(room, start);
    if (vis < 0) {
        if (start && LogQuota(&g_rmQuotaAnom, 20))
            LogAnomaly("randomizer", "monsters: room %d:%d: its script (%08X) is in neither half's scan: its monsters are left alone",
                       stage, room, start);
        return;
    }
    if (!g_rmVisitOk[vis]) return;
    for (i = 0; i < g_rmScan[vis].n && m < RM_ROOM_MAX; ++i)
        if (g_rmScan[vis].s[i].room == room) idx[m++] = i;  // (script order: the scan walks a room's objects in order)
    if (!m) return;
    ct = RmCtOn();
    if (ct != g_rmCt) {
        if (ct || g_rmCt > 0)
            Log("randomizer: monsters: the cheat table's Enemy Randomizer (entry 7700: 0052CAA9 / 0052CB2B) is %s: Refined's "
                "Monsters %s", ct ? "on" : "off again", ct ? "stay off while it is (it retypes the placements itself)" : "apply again");
        g_rmCt = ct;
    }
    if (ct) {  // (touch nothing: no word, no record; the ledger forgets this room)
        ++g_rmCtLoads;
        for (k = 0; k < m; ++k) g_rmCurT[vis][idx[k]] = 0, g_rmCurV[vis][idx[k]] = 0;
        return;
    }
    if (mode > RMM_OFF && mode < RMM_N) {
        if (HpTotal() <= HP_STOCK_N) why = 1;
        else if (g_rmScan[vis].roomBoss[room]) why = 2;
        else if (!EnmRoomForOne()) why = 3;
        if (why == 1 && !g_rmHitSaid++)
            Log("randomizer: monsters: the hit-shape pool is the stock 100 ([Gameplay] HitPool): Monsters stay as placed (a new type "
                "may need more shapes than the room's own)");
    }
    if (mode <= RMM_OFF || mode >= RMM_N || why) {
        const int n = RmDropForeign(stage, half, room, idx, m, vis);
        g_rmOffDrops += n;
        if (n) Log("randomizer: monsters: room %d:%d: %d record(s) of monsters made as another type before are deleted (they come "
                   "back as placed)", stage, room, n);
        if (why == 2 || why == 3)
            LogV("randomizer: monsters: room %d:%d left as placed (%s)", stage, room,
                 why == 2 ? "a boss room" : "the enemy list / task / actor pools are nearly full");
        if (SsOn() && why != 2 && !g_rmScan[vis].roomBoss[room]) RmSitesStock(stage, half, room, d, vis, idx, m);  // v3.56 RE356
        return;
    }
    if (!g_rmFirstSaid++)
        Log("randomizer: monsters: the first room with source placements (stage %d room %d): Enemies %s", stage, room,
            kRmMonName[mode]);  // v3.56 RE356: Enemies (was Monsters)
    // the plan's inputs: the records, the floors under the Hummers, the pool
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &g_rmScan[vis].s[idx[k]];
        const BYTE *r = RmRecLive(EnmScriptId(stage, half, room, s->n));
        rec[k].type = r && RmRecStage(r) == stage ? RmRecType(r) : -1;
        rec[k].variant = r ? RmRecVariant(r) : 0;
        rec[k].here = r && RmRecRoom(r) == room;
        rec[k].ours = RmProvOurs(&g_rmProv, r);  // v3.49 fix2: only Refined's own records are kept, followed or deleted
        floorOk[k] = 0, floorY[k] = s->pos[1];
        if (RmIsFlyer(s->type)) {
            float base[3], cand[3];
            memcpy(base, s->pos, 12), memcpy(cand, s->pos, 12);
            if (EnmFinite3(base) && EnmSpotTest(base, cand, 0, 0.0f) == ENM_SPOT_OK && _finite(cand[1])) floorOk[k] = 1, floorY[k] = cand[1];
        }
    }
    memset(&in, 0, sizeof in);
    in.mode = mode, in.seed = (unsigned)g_rzSeed, in.stage = stage, in.half = half, in.room = room;
    in.visit = RzVisitSalt(stage, room);  // v3.56 RI356: By Room = the room's entries, By World = the world visit (worldvisit.inc)
    in.reroll = RZ_PERVISIT_ON();  // Change Each Visit: the monsters left in this room roll again too  // v3.56 RI356: By Room (By World: a record keeps its type, as v3.55's Off)
    in.allowed = RM_ALL_TARGETS & (RmGumGuard() ? RM_ALL_TARGETS : ~RM_GUM_TARGETS);
    {  // v3.56 RE356: the victims' ghosts whose create writes a story flag only once that flag is set (rzmon_sim.h RmStoryAllowed)
        const int f2D3 = (*(volatile const BYTE *)0x13D80FA & 8) != 0, f23B = (*(volatile const BYTE *)0x13D80E7 & 8) != 0;
        const unsigned story = RmStoryAllowed(stage, f2D3, f23B);
        if ((RM_ALL_TARGETS & ~story) && !(g_rmStorySaid[stage] >> (f2D3 | f23B << 1) & 1u)) {
            g_rmStorySaid[stage] |= 1u << (f2D3 | f23B << 1);
            Log("randomizer: monsters: story rule in stage %d: %s%s%s (their create sets that flag; in the pool once it is set)", stage,
                stage == 4 && !f2D3 ? "no male victim ghost (0B) before story flag 2D3" : "",
                stage == 4 && !f2D3 && !f23B ? "; " : "", !f23B ? "no old man's ghost (0D v1) before story flag 23B" : "");
        }
        in.allowed &= story;
    }
    in.world = g_rmScan[0].placed | (g_rmScanHas1 ? g_rmScan[1].placed : 0u);
    for (i = 0; i < RM_TARGET_N; ++i)
        if (EnmLoadedHere(kRmTargets[i].type, kRmTargets[i].variant)) in.resident |= 1u << i;
    in.freeBones = *(volatile const int *)0xED73E4, in.totalBones = *(volatile const int *)0xED73EC;
    in.released = ENM_DOGS_AWAKE, in.rollAgain = g_rmRollAgain;  // v3.51 SN351 (story flag 0x130: enemies.inc)
    RmPlanRoom(&in, &g_rmScan[vis], idx, m, rec, floorOk, pick, &st);
    // This World: the files of each target not resident here (EnmEnsureFiles; a failure puts its placements back)
    for (i = 0; i < RM_TARGET_N; ++i) {
        int need = 0, kbi = 0;
        if (in.resident >> i & 1u) continue;
        for (k = 0; k < m; ++k) need |= pick[k].type == kRmTargets[i].type && pick[k].variant == kRmTargets[i].variant;
        if (!need) continue;
        if (EnmEnsureFiles(kRmTargets[i].type, kRmTargets[i].variant, &kbi)) {
            ++loaded, kb += kbi;
            continue;
        }
        if (LogQuota(&g_rmQuotaFiles, 200))
            Log("randomizer: monsters: room %d:%d: %s's files could not be loaded (%s): its placements stay as placed", stage, room,
                kRmTargets[i].name, g_enmFilesWhy);
        for (k = 0; k < m; ++k)
            if (pick[k].type == kRmTargets[i].type && pick[k].variant == kRmTargets[i].variant)
                RmRevertPick(&pick[k], &g_rmScan[vis].s[idx[k]], &rec[k], &st);
    }
    // the words (a slot's mode and y words first, its type word last: a word that is not the stock value puts the slot back)
    list[0] = 0;
    // v3.56 RE356: Spawn Site: each placement's site for the type it is made as (spawnsite.inc SsMonSite), before any word
    if (SsOn()) {
        for (k = 0; k < m; ++k) {
            const RmSlot *s = &g_rmScan[vis].s[idx[k]];
            const float hover = RmIsFlyer(s->type) && RmIsFlyer(pick[k].type) && floorOk[k] ? floorY[k] - s->pos[1] : 0.0f;
            int keep;
            sited[k] = (unsigned char)SsMonSite(stage, half, room, s, pick[k].type, g_rmScan[vis].roomBoss[room], in.released,
                                                rec[k].type >= 0 && !rec[k].here && !pick[k].drop && !RmIsGhostTarget(pick[k].type),
                                                hover, site[k], &keep);  // (a ghost's record elsewhere: 0x482590 drops it half the time)
        }
    }
    for (k = 0; k < m; ++k) {
        const RmSlot *s = &g_rmScan[vis].s[idx[k]];
        RmPick *p = &pick[k];
        const int ch = p->type != s->type || p->variant != s->variant;  // v3.56 RE356
        if (ch || sited[k]) {  // v3.56 RE356: + a site of its own (its x / y / z words)
            const int n0 = d >= 0 && d < RZ_DEPTH ? g_rmStack.n[d] : 0;
            const unsigned mw = RmModeWord(s->type, s->mw, p->type, p->variant);
            float want[3];
            int ok = 1;
            memcpy(want, sited[k] ? site[k] : s->pos, 12);
            if (!sited[k] && ch && RmIsFlyer(s->type) && !RmIsFlyer(p->type)) want[1] = floorY[k];  // a walker at a Hummer's spot
            if (ch && mw != s->mw) ok = RmStackAdd(&g_rmStack, d, (unsigned *)(ULONG_PTR)s->modeVa, s->mw, mw);
            if (ok) ok = RmPosWords(d, s, want);
            if (ok && ch) ok = RmStackAdd(&g_rmStack, d, (unsigned *)(ULONG_PTR)s->typeVa, s->type, p->type);
            if (!ok) {
                RmStackTrim(&g_rmStack, d, n0);
                ++other;
                if (sited[k]) --g_ssRmMoved, ++g_ssRmKept;  // v3.56 RE356 (its site's count back)
                sited[k] = 0;
                if (ch) RmPickBack(p, s, &rec[k], &st, RMW_WORDS);  // v3.50 RM350: the shared rule (an adopted record stays)
            }
        }
        if (p->drop) {  // (a record of another type than the one it is made as now: that monster comes back fresh)
            BYTE *r = RmRecLive(EnmScriptId(stage, half, room, s->n));
            if (r && RmRecRoom(r) == room) ENM_RECDEL(r);
        }
        if (sited[k] && !p->drop)  // v3.56 RE356: its standing record here goes to the site too (else the game puts it back)
            SsMonRecMove(EnmScriptId(stage, half, room, s->n), stage, room, p->type, p->variant, site[k]);
        // v3.51 SN351: an event dog left asleep is not "built" by Refined: its record stays the game's own, so the first visit
        // after its release adopts and rolls it (else Change Each Visit Off would keep Refined's stock record: a dog for good)
        if (p->why != RMW_EVENT) RmProvAfterBuild(stage, half, room, s, p->type, p->variant);  // v3.49 fix2 RZ349-SAVE-001
        g_rmCurT[vis][idx[k]] = p->type != s->type || p->variant != s->variant ? p->type : 0;
        g_rmCurV[vis][idx[k]] = p->variant;
        if ((p->type != s->type) && !(g_rmTypes >> p->type & 1u) && !(g_rmSilent[stage] >> p->type & 1u)) {
            g_rmSilent[stage] |= 1u << p->type;
            Log("randomizer: monsters: %s is no monster of stage %d's scripts: its sounds may be missing there (allowed)",
                RmTypeName(p->type), stage);
        }
        if (o < (int)sizeof list - 100) {  // v3.50 RM350: every kept placement says why; an adopted one says so
            const int ch = p->type != s->type || p->variant != s->variant;
            const char *why = p->why == RMW_STICKY ? "its record" : p->why == RMW_BONES ? "bones" : p->why == RMW_FILES ? "files"
                              : p->why == RMW_FLOOR ? "no floor" : p->why == RMW_AWAY ? "recorded elsewhere"
                              : p->why == RMW_OTHER ? "another writer's record" : p->why == RMW_NONE ? "no target" : p->why == RMW_WORDS ? "words not stock"
                              : p->why == RMW_EVENT ? "asleep for its event"  // v3.51 SN351: + the event dogs, the second roll
                              : p->why == RMW_ROLL ? (ch ? (p->again ? "rolled again" : "") : (p->again ? "rolled it twice" : "rolled"))
                              : "its stock record";
            const int w = _snprintf_s(list + o, sizeof list - o, _TRUNCATE, "%sn%d %s v%d%s%s v%d%s%s%s%s%s", o ? "; " : "", s->n,
                                      RmTypeName(s->type), s->variant, ch ? " -> " : " kept as ", RmTypeName(p->type), p->variant,
                                      *why ? " (" : "", why, *why ? ")" : "", p->adopt ? (p->drop ? " [the game's record, adopted and "
                                      "deleted]" : " [the game's record, adopted]") : "",
                                      sited[k] ? " at a new site" : "");  // v3.56 RE356
            o = w < 0 ? (int)sizeof list - 1 : o + w;  // (_TRUNCATE: a full list ends there, never the invalid-parameter handler)
        }
    }
    if (SsOn()) SsMonRoomSay(stage, room);  // v3.56 RE356
    for (k = 0, drops = 0; k < m; ++k) drops += pick[k].drop;
    RmOtherSay(stage, room, st.other);  // v3.49 fix2
    ++g_rmRooms;
    g_rmChanged += st.changed, g_rmSticky += st.sticky, g_rmKeptBones += st.bones, g_rmKeptFiles += st.files;
    g_rmKeptFloor += st.floor, g_rmFileTypes += loaded, g_rmFileKb += kb, g_rmLeft += other, g_rmDrops += drops;
    g_rmAdopted += st.adopted, g_rmAdoptChanged += st.adoptChanged, g_rmRollStock += st.rollStock, g_rmSame += st.same;  // v3.50 RM350
    g_rmAway += st.away, g_rmNone += st.none;
    g_rmAgain += st.again, g_rmEvPlanned += st.eventPlanned, g_rmEvAsleep += st.event;  // v3.51 SN351
    if (LogQuota(&g_rmQuotaRoom, 4000)) {
        char by[300], tail[160];  // v3.51 SN351: + tail (only when a placement rolled again or the room has event dogs)
        int b = 0, t, e = 0;
        by[0] = 0, tail[0] = 0;
        if (st.again) e += sprintf_s(tail + e, sizeof tail - e, "; rolled again for their own type: %d", st.again);
        if (st.event + st.eventPlanned)
            e += sprintf_s(tail + e, sizeof tail - e, "; event dogs: %d, story flag 130 %s", st.event + st.eventPlanned,
                           in.released ? "set (rolled as the others)" : "clear (kept as placed for their event)");
        for (t = 0; t < 0x13; ++t) {
            int c = 0;
            for (k = 0; k < m; ++k) c += pick[k].type == t;
            if (c) b += sprintf_s(by + b, sizeof by - b, "%s%s %d", b ? ", " : "", RmTypeName(t), c);
        }
        Log("randomizer: monsters: room %d:%d (visit %d, #%u): %d of %d placement(s) changed (%s): %s; %d kept their record's type, "
            "%d record(s) deleted; kept as placed: %d for the bone budget, %d for files, %d without a floor%s; files loaded for %d "
            "type(s) (%d KB); the game's own records: %d adopted (%d made as another type); stock type kept: %d rolled it, %d by a "
            "stock record, %d recorded elsewhere, %d for another writer's record, %d with no target%s", stage, room, vis, in.visit,
            st.changed, m, kRmMonName[mode], by, st.sticky, drops, st.bones, st.files, st.floor,
            other ? "; some words were not the stock value: left as found" : "", loaded, kb, st.adopted, st.adoptChanged,
            st.rollStock, st.same, st.away, st.other, st.none, tail);  // v3.50 RM350: + the adopted records and the reasons  // v3.51 SN351: + tail
    }
    if (other && LogQuota(&g_rmQuotaAnom, 20))
        LogAnomaly("randomizer", "monsters: room %d:%d: %d placement word(s) did not hold the stock value (another mod?): left as found",
                   stage, room, other);
    LogV("randomizer: monsters: room %d:%d: %s", stage, room, list);
}
// v3.48 fix1 RZ-002's rule for these words too: one word list per room-script run (its own level), restored in the
// __finally after the runner (randomizer.inc RzAfterScript -> RmAfterScript), the deepest first
static void __cdecl RmBeforeScript(int room)
{
    const int d = RmStackPush(&g_rmStack);
    __try {  // v3.56 RE356: Spawn Site's room state for this run (its own fault flag)
        SsLoadBegin();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        SsFault(GetExceptionCode());
    }
    if (d >= RZ_DEPTH || g_rmFail) return;
    __try {
        RmBeforeScriptC(room, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        int left = 0;
        __try {
            left = RmStackTrim(&g_rmStack, d, 0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        g_rmFail = 1;
        g_ssFail = 1;  // v3.56 RE356: (its monster sites run in here: both off, fail closed)
        LogAnomaly("randomizer", "monsters: faulted at a room load (code %08lX; %d word(s) not restored): Enemies and Spawn Site's "
                   "monster sites are off for this session", GetExceptionCode(), left);
    }
}
static void __cdecl RmAfterScript(void)
{
    const int d = g_rmStack.depth - 1;
    int other = 0;
    __try {
        other = RmStackPop(&g_rmStack, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rmStack.depth = d < 0 ? 0 : d;
        g_rmFail = 1;
        LogAnomaly("randomizer", "monsters: faulted restoring the placement words (code %08lX): Monsters are off for this session",
                   GetExceptionCode());
    }
    g_rmLeft += other;
    if (other && LogQuota(&g_rmQuotaAnom, 20))
        LogAnomaly("randomizer", "monsters: %d placement word(s) changed while the room was built (another writer): left as found", other);
}

// ---------------------------------------------------------------- rebuilt placements (enemies.inc EnmWalkC)
// Enemy Copies (this room, right after the runner), Enemies Follow's and the wanderers' replays and Revive Enemies rebuild a
// placement from its script (EnmWalk: 0x52C530 over the stock words): it gets the type it lives as. Its record's type when
// the game holds a record of it that Refined made (a follower, a wanderer, a monster left in its room), else this world
// visit's ledger (the type it was last made as; a copy of a retyped original is the same new type). v3.49 fix2: a record
// Refined did not make leaves the placement as the script says.
static void RmDescFix(int room, int n, BYTE *desc)
{
    if (g_rmFail || g_rmScanStage < 0 || ENM_STAGE != g_rmScanStage || room <= 0 || room >= RZ_ROOMS) return;
    __try {
        const int stage = ENM_STAGE, half = ENM_HALF() ? 1 : 0;
        const int vis = RmVisOf(room, ((const DWORD *)0x10A0A78)[room]);
        const RmSlot *s;
        const BYTE *r;
        int i, type, variant;
        if (vis < 0 || !g_rmVisitOk[vis] || *(const int *)desc != 2 || (i = RmSlotOf(&g_rmScan[vis], room, n)) < 0) return;
        s = &g_rmScan[vis].s[i];
        r = RmRecLive(EnmScriptId(stage, half, room, n));
        if (r && !RmProvOurs(&g_rmProv, r)) return;  // v3.49 fix2: another writer's record: as placed
        if (r && RmTargetIndex(RmRecType(r), RmRecVariant(r)) >= 0) type = RmRecType(r), variant = RmRecVariant(r);
        else if (g_rmCurT[vis][i]) type = g_rmCurT[vis][i], variant = g_rmCurV[vis][i];
        else return;
        RmFixDesc(desc, s, type, variant, (const unsigned *)(ULONG_PTR)(RM_TYPE_TAB + 16u * (unsigned)type));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rmFail = 1;
        LogAnomaly("randomizer", "monsters: faulted rebuilding a placement (code %08lX): Monsters are off for this session",
                   GetExceptionCode());
    }
}

// ---------------------------------------------------------------- the save filter (0x465468)
// EnemyManagerSave 0x465410 writes a record for every enemy of the list, copies the 0x1B90 bytes of gEnemyManager from
// its kill bits 0xFCF010 into the save (`call 42BBB0` at 0x465468) and deletes those records again. The re-aimed call
// makes the same copy, then clears in the COPY every record of a monster Refined randomized and that still holds exactly
// the type Refined made it as (v3.49 fix2 RZ349-SAVE-001: rzmon_sim.h RmProvRetyped; was every record whose type was not
// the stock one): a save (also the death-continue snapshot, Quick Save) never holds a monster Refined randomized; it comes
// back fresh as its placement says. Every other record stays (another writer's too). Live records stay. While Refined has
// randomized nothing this session (Monsters Off from the start) the copy is the game's, byte for byte. v3.50 RM350: an
// adopted record (the game's own stock one) is Refined's after its room's plan (RmProvAfterBuild): kept stock it stays in
// saves (it is the stock monster); rolled another type it was deleted, and the new type's record is left out as above.
static void __cdecl RmSaveCopy(void *dst, const void *src, unsigned size)
{
    ((void(__cdecl *)(void *, const void *, unsigned))0x42BBB0)(dst, src, size);
    __try {
        BYTE *r = (BYTE *)dst + RM_SAVE_RECS;
        int i, n = 0;
        if (size != 0x1B90 || src != (const void *)0xFCF010 || RmCtOn()) return;  // (the cheat table's 7700 on: Monsters are off)
        ++g_rmSaves;
        for (i = 0; i < RM_REC_N; ++i, r += RM_REC_SIZE)
            if (RmProvRetyped(&g_rmProv, r) && LogQuota(&g_rmQuotaAnom, 40))
                LogV("randomizer: monsters: a save: %08X (made as %s v%d, placed as another type) is left out of it", RmRecId(r),
                     RmTypeName(RmRecType(r)), RmRecVariant(r));
        n = RmSaveClean((BYTE *)dst + RM_SAVE_RECS, &g_rmProv);
        g_rmSaveCleaned += n;
        if (n) Log("randomizer: monsters: a save: %d record(s) of monsters made as another type are left out of it (they come back "
                   "as placed after a load)", n);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        if (LogQuota(&g_rmQuotaAnom, 20))
            LogAnomaly("randomizer", "monsters: faulted cleaning a save's records (code %08lX): that save is as the game made it",
                       GetExceptionCode());
    }
}

// ---------------------------------------------------------------- start and end
static void RmInit(void)  // randomizer.inc RzInit (GameplayInstall: before the Options tables; after RzReadIni)
{
    // EnemyManagerSave: mov eax,[ebp+8] .. push 1B90h; push 0FCF010h; push eax; call 42BBB0
    static const Site kSave = { 0x46545D, { 0x68, 0x90, 0x1B, 0x00, 0x00, 0x68, 0x10, 0xF0, 0xFC, 0x00, 0x50, 0xE8, 0x43, 0x67, 0xFC,
                                            0xFF }, 16 };
    int stage, v, scripts = 0, total = 0, kind2 = 0, i;
    unsigned seen[RZ_STAGES * 2];
    static RmScan s_sc;
    RmTexts();
    if (!g_iniFallback) g_rmSaveIni = GetPrivateProfileIntA("Randomizer", "MonsterSaveFilter", 1, g_iniPath) != 0;  // (hidden)
    if (!g_iniFallback) g_rmRollAgain = GetPrivateProfileIntA("Randomizer", "MonsterRollAgain", 1, g_iniPath) != 0;  // v3.51 SN351 (hidden)
    g_rmSelfEv = 0;  // v3.51 SN351
    __try {
        for (stage = 0; stage < RZ_STAGES; ++stage)
            for (v = 0; v < 2; ++v) {
                const unsigned p = RzScript(stage, v);
                int k, dup = 0, m;
                for (k = 0; k < scripts; ++k) dup |= seen[k] == p;
                if (dup) continue;
                seen[scripts++] = p;
                RmScanScript(RzWindow(), p, stage, &s_sc);
                total += s_sc.n, kind2 += s_sc.kind2;
                g_rmSelfEv += s_sc.events;  // v3.51 SN351
                m = RmStockMatch(stage, v, p, &s_sc);
                if (m == 0 || (m < 0 && (s_sc.n || s_sc.bad))) ++g_rmSelfBad;
            }
        g_rmSelfN = total, g_rmSelfK = kind2, g_rmScripts = scripts;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rmSelfN = -1;
        g_rmFail = 1;
    }
    if (g_rmSelfN >= 0 && g_rmSaveIni && SiteOk(&kSave)) {
        GpPatch p;
        GpBranch(&p, 0x465468, 0xE8, RmSaveCopy, 5);
        g_rmSaveSite = GpApplyAll(&p, 1, "randomizer (monsters' save filter)");
    }
    {
        char t[400];  // v3.56 RE356: 15 targets (was 200)
        int o = 0;
        for (i = 0; i < RM_TARGET_N; ++i)
            o += sprintf_s(t + o, sizeof t - o, "%s%s", i ? ", " : "", kRmTargets[i].name);
        Log("randomizer: [Randomizer] Enemies %s; self-check: %d source placement(s) of %d enemy placement(s) in %d script(s) "
            "(stock %d / %d)%s; targets %s (Gum Heads only with a crash guard; victims' ghosts by the story rule); applied at each room load through 004F0AF8 "
            "(patch-and-restore of the type / mode / position words); save filter 00465468 %s", kRmMonName[g_rzMon >= 0 && g_rzMon < RMM_N ?
            g_rzMon : 0], g_rmSelfN, g_rmSelfK, g_rmScripts, RM_STOCK_TOTAL, RM_STOCK_KIND2,
            g_rmFail ? ": the scan FAULTED, Monsters are off" : g_rmSelfBad ? ": some scripts are not the stock ones (their monsters "
            "are left alone)" : ": the stock scripts", t, g_rmSaveSite ? "in" : !g_rmSaveIni ? "off ([Randomizer] MonsterSaveFilter=0)"
            : "NOT in (unexpected bytes: a save may hold randomized monsters)");
    }
    if (g_rmSelfN >= 0)  // v3.51 SN351
        Log("randomizer: monsters: %d of the source placements are event Sniffer Dogs (spawn mode 6 / 7, stock: Subway room 2:2's "
            "%d): rolled once story flag 130 is set (their cut-scene is over), else as placed; a roll of the placement's own type "
            "rolls once more: %s ([Randomizer] MonsterRollAgain)", g_rmSelfEv, RM_STOCK_EVENTS, g_rmRollAgain ? "On" : "Off");
}
static void RmSessionLine(void)  // randomizer.inc RzSessionLine
{
    Log("randomizer: monsters session: %s (seed %lu, Change Each Visit %s): %d room load(s), %d with source placements randomized, "
        "%d placement(s) changed, %d kept their record's type, %d record(s) deleted (+%d with Monsters off); kept as placed: %d for "
        "the bone budget, %d for files, %d without a floor; files loaded %d time(s) (%d KB); %d room load(s) with the cheat "
        "table's Enemy Randomizer on; %d save(s), %d record(s) left out; %d word(s) left as found%s",
        kRmMonName[g_rzMon >= 0 && g_rzMon < RMM_N ? g_rzMon : 0], (unsigned long)(unsigned)g_rzSeed, kRzKeepName[RZ_PERVISIT_ON() ? 1 : 0] /* v3.56 RI356: On = Reroll By Room */,
        g_rmLoads, g_rmRooms, g_rmChanged, g_rmSticky, g_rmDrops, g_rmOffDrops, g_rmKeptBones, g_rmKeptFiles, g_rmKeptFloor,
        g_rmFileTypes, g_rmFileKb, g_rmCtLoads, g_rmSaves, g_rmSaveCleaned, g_rmLeft,
        g_rmFail ? "; FAULTED (off)" : !g_enmInstalled ? "; no room-load hook (off)" : "");
    if (g_rmRooms)  // v3.50 RM350
        Log("randomizer: monsters session: the game's own records: %d adopted (%d made as another type); stock type kept: %d rolled "
            "it, %d by a stock record, %d recorded elsewhere, %d with no target", g_rmAdopted, g_rmAdoptChanged, g_rmRollStock, g_rmSame,
            g_rmAway, g_rmNone);
    if (g_rmRooms)  // v3.51 SN351
        Log("randomizer: monsters session: %d placement(s) rolled again for their own type (MonsterRollAgain %s); event dogs: %d "
            "planned with story flag 130 set, %d kept as placed for their event (flag clear)", g_rmAgain, g_rmRollAgain ? "On" : "Off",
            g_rmEvPlanned, g_rmEvAsleep);
    if (g_rmOther || g_rmProv.full)  // v3.49 fix2 RZ349-SAVE-001
        Log("randomizer: monsters session: %d placement(s) left as placed for a record Refined did not make (another writer); %d "
            "placement(s) of Refined's (%d full)", g_rmOther, g_rmProv.n, g_rmProv.full);
}
