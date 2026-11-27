// ================================================================ v3.49 RZ349L: the Randomizer's looks (Option 5 > Randomizer)
// Plan: staging/v349_feedback/w4_looks/PLAN.md (the research: staging/research_randomizer/REPORT.md sections 3 b1, 5 d1).
// Two rows on the Randomizer page (randomizer.inc RzBuildPage, after the Items rows; the merge puts them after w3's
// Monsters row), the pure part in rzlooks_sim.h. Included by sh4windowmod.c after enemies.inc: it needs
// eileenoutfit.inc's engine (eo_outfit, g_eoSaved, eo_scan, eo_api) and enemies.inc's spot test (EnmSpotTest,
// EnmPlayerPos); randomizer.inc declares its entry points.
//
// [Randomizer] EileenOutfit Off / Random (default Random): companion Eileen's outfit at random among the ones the save
//   has unlocked (Dress always, Nurse with gSaveNurseUnlocked 0x13D1A50), once per world visit (Items' rule: a visit
//   starts when the stage changes), seeded (Change Each Visit Off: the same outfit for this seed, stage and half). It
//   only sets the outfit engine's eo_outfit (as the Extra Menu row does, but never writes [EileenOutfit] Outfit): the
//   engine's own hooks load and put on the outfit. Drawn at the world's file list (eo_stage 0x4F3490: the engine then
//   loads both models) or, switched on in the middle of a visit, at the next room load (eo_room 0x4F3514). A choice in
//   the Extra Menu is kept until the next world visit; Off, or a world without her models, puts the set outfit back.
//   Playable Eileen owns the PLAYER's look (its [Variant] Outfit, henry01 banks, SH4PE_RequestVariant) and leaves the
//   companion to the game: this row changes the companion only (BUILD_NOTES: a contract for the player, not built).
// [Randomizer] Ghosts Off / Spots (default Spots): every victim ghost placement of the room being built (rzlooks_sim.h
//   RzgMayMove: 0B, 0C v1, 0D v1, spawn mode 0, its FFFF00nn id; never the story ghosts) starts at another spot of the
//   same room: its three cmd 8 position words are written right before the stock runner 0x52CA00 reads the room's script
//   and put back right after it (randomizer.inc's patch stack, this run's level). The spot: one of the room's other
//   placements or three rings around it (600-2200 units), each tested as enemies.inc tests a flying copy (EnmSpotTest
//   ghost 2: a floor of the same storey under it, the same height above it, not in a wall, in sight of the stock
//   spot), at least 300 from the stock spot, 250 from another moved victim and 600 from where you arrive. A victim the
//   game restores from its record keeps that place (the record wins over the script); nothing is saved.
//
// v3.50 RL350 (staging/v350_feedback/w3_looks/PLAN.md; the user's v3.49 QA: "make her outfit change per room and remove
// the unlock limiter"; Ghosts Spots "Fail"): Eileen Outfit draws among every companion outfit (Dress, Nurse: the Nurse's
// unlock 0x13D1A50 is no longer asked) and, with Change Each Visit On, again at every room load of her world (the room
// load after the world's file list keeps that draw; at most 2 room loads in a row in one outfit); Off = one draw per
// world visit as before. The switch is the engine's room-load path (eo_room puts the outfit into 0x13D19EC, her model is
// built from it at the room's spawn, eo_gate 445BDB falls back to the other resident model: never invisible); both models
// are resident in her worlds (eo_stage loads both while eo_outfit >= 0). Ghosts: v3.49's log moved 9 of 9, the user saw
// none: (1) two victims of a room swapped places (the room's other placements were candidates; 2:7 #2 / #3), (2) most
// moves were 400-600 units, (3) a victim's own enemy record (written as its room was left; also in a save) is restored at
// its spot half the time (EnemyGhostReconcile 0x482590, gate 4). Now: never within 600 of another ghost's placement, at
// least 500 from its own and 800 + (1000 + from where you arrive) when the room allows, and a standing record of it in
// this room is moved to the new spot too (pinned / down ones keep theirs); the log gives the distance and the record.

#include "rzlooks_sim.h"
#include "spawnsite_sim.h"  // v3.56 RE356: (its pure part; teleport_sim.h with it)
// v3.56 RE356: Spawn Site (spawnsite.inc, included after teleport.inc: it uses Ghost Teleport's floor map and queries)
static int SsGhostSite(int stage, int half, int room, int n, int kind, const float *base, float hover, const float (*ghosts)[3],
                       int nGhosts, float *out, int *why);
static void SsTake(const float *p);
static void SsSessionLine(void);

static volatile LONG g_rzoMode = RZO_DEFAULT, g_rzgMode = RZG_DEFAULT;
static LONG g_rzlSnap[2];        // as Options opened
static int g_rzlUnsaved;         // the menu's value could not be written (the poll leaves the file's value alone)
static int g_rzoFail, g_rzgFail;  // faulted: that half is off for this session
static RzoState g_rzo;
static int g_rzlFirstSaid, g_rzgCtSaid;
static int g_rzgRooms, g_rzgMoved, g_rzgKept, g_rzgLeft, g_rzgVictims;
static int g_rzgFar, g_rzgRecMoved, g_rzgRecKept;  // v3.50 RL350: moves of 800 +, records moved, pinned / down records kept
static volatile LONG g_rzoQuota, g_rzgQuota, g_rzgQuotaAnom;
static volatile LONG g_rzoQuotaRoom;  // v3.50 RL350: the room draws' lines
// v3.50 RL350: the Nurse's unlock check (SaveGetNurseUnlocked 0x56F590, kRzoNurseFn) is gone: every outfit (RZO_ALL)

// ---------------------------------------------------------------- texts (MSG_RZ1 + 16 .. + 31: this worker's half)
#define MSG_RZL0 (MSG_RZ1 + 16)
enum { RZLM_LO, RZLM_VR, RZLM_DO0, RZLM_DO1, RZLM_LG, RZLM_VS, RZLM_DG0, RZLM_DG1, RZLM_N };
typedef char RzlTextsFit[MSG_RZL0 + RZLM_N <= MSG_V50 ? 1 : -1];  // v3.50 MERGE: bounded by the next block (was MSG_END)
typedef char RzlRowsFit[(6 + 2 + 1) * 7 + 1 <= sizeof g_rowsRando / sizeof g_rowsRando[0] ? 1 : -1];  // + w3's Monsters: 10
static BYTE g_txtRzl[RZLM_N][64];
static DWORD g_rzoIds[2], g_rzgIds[2];
static const BYTE *RzlText(int id)
{
    return id >= MSG_RZL0 && id < MSG_RZL0 + RZLM_N ? g_txtRzl[id - MSG_RZL0] : RzText(MSG_RZ0 + RZM_NA);
}
static void RzlTexts(void)  // randomizer.inc RzTexts
{
    // v3.53 OL1: the plan's lines (+ "Next room.")  // v3.56 RI356: without "Next room." (the plain _D / _VD lines)
    Encode(g_txtRzl[RZLM_LO], 64, OLW_RANDO_EILEEN_OUTFIT_L);
    Encode(g_txtRzl[RZLM_VR], 64, OLW_RANDO_EILEEN_OUTFIT_V_RANDOM);
    Encode(g_txtRzl[RZLM_DO0], 64, OLW_RANDO_EILEEN_OUTFIT_D);
    Encode(g_txtRzl[RZLM_DO1], 64, OLW_RANDO_EILEEN_OUTFIT_VD_RANDOM);  // v3.50 RL350 (was "an unlocked outfit each world visit")
    Encode(g_txtRzl[RZLM_LG], 64, OLW_RANDO_SPAWN_SITE_L);        // v3.56 RE356: Spawn Site (was Ghosts), no "Next room." tag
    Encode(g_txtRzl[RZLM_VS], 64, OLW_RANDO_SPAWN_SITE_V_ON);
    Encode(g_txtRzl[RZLM_DG0], 64, OLW_RANDO_SPAWN_SITE_VD_OFF);
    Encode(g_txtRzl[RZLM_DG1], 64, OLW_RANDO_SPAWN_SITE_VD_ON);
    g_rzoIds[0] = g_rzgIds[0] = MSG_RZ0 + RZM_VK0;  // "Off" (the Keep Specials row's)
    g_rzoIds[1] = MSG_RZL0 + RZLM_VR;
    g_rzgIds[1] = MSG_RZL0 + RZLM_VS;
}

// ---------------------------------------------------------------- [Randomizer] EileenOutfit / Ghosts
static const char *const kRzlKey[2] = { "EileenOutfit", "SpawnSite" };  // v3.56 RE356: Ghosts -> SpawnSite (RzReadKeyMig reads the old key once)
static volatile LONG *RzlSetting(int k) { return k ? &g_rzgMode : &g_rzoMode; }
static const char *RzlValName(int k, LONG v) { return k ? kRzgName[v ? 1 : 0] : kRzoName[v ? 1 : 0]; }
static void RzlReadIni(void)  // randomizer.inc RzReadIni (RzInit): the keys and the start line
{
    g_rzoMode = RzReadKey("EileenOutfit", RzoOfText, RZO_DEFAULT, kRzoName, "Off / Random");
    g_rzgMode = RzReadKeyMig("SpawnSite", "Ghosts", RzgOfText, RZG_DEFAULT, kRzgName, "Off / On");  // v3.56 RE356 (was Ghosts)
    RzoInit(&g_rzo);
    Log("randomizer: looks: [Randomizer] EileenOutfit %s, SpawnSite %s (companion Eileen's outfit among every outfit, Dress and "
        "Nurse: at every room load of her world with Change Each Visit On, else once per world visit; every monster and victim "
        "a room places at a site of the room: monsters on floor you can walk to, ghosts anywhere in the room's box, their "
        "records in the room moved too: patch-and-restore of their position words, no exe patch)", kRzoName[g_rzoMode ? 1 : 0],
        kRzgName[g_rzgMode ? 1 : 0]);  // v3.50 RL350  // v3.56 RE356: Spawn Site
}
static void RzlSetLocked(int k, LONG v)  // GpLock held
{
    const LONG was = *RzlSetting(k);
    InterlockedExchange(RzlSetting(k), v);
    g_rzlUnsaved = !RzWrite(kRzlKey[k], RzlValName(k, v)) && !g_iniFallback;
    Log("randomizer: %s %s (was %s; applies at the next room load)", kRzlKey[k], RzlValName(k, v), RzlValName(k, was));
}
static void RzlIniReread(void)  // randomizer.inc RzIniReread (under GpLock)
{
    int (*const parse[2])(const char *) = { RzoOfText, RzgOfText };
    char b[64];
    int k, v;
    if (g_iniFallback || g_rzlUnsaved) return;
    for (k = 0; k < 2; ++k) {
        volatile LONG *const t = RzlSetting(k);
        GetPrivateProfileStringA("Randomizer", kRzlKey[k], "", b, sizeof b, g_iniPath);
        v = parse[k](b);
        if (v < 0 || v == (int)*t) continue;
        InterlockedExchange(t, v);
        g_rzlSnap[k] = v;
        Log("randomizer: [Randomizer] %s read again: %s (applies at the next room load)", kRzlKey[k], RzlValName(k, v));
    }
}
static void RzlSnapshot(void) { g_rzlSnap[0] = g_rzoMode, g_rzlSnap[1] = g_rzgMode; }
static int RzlAnyOn(void) { return g_rzoMode != RZO_OFF || g_rzgMode != RZG_OFF; }  // v3.56 RI356: randomizer.inc RzAnyOn (the seed line)
static int RzlChanged(void) { return g_rzoMode != g_rzlSnap[0] || g_rzgMode != g_rzlSnap[1]; }
static void RzlDiscard(void)  // under GpLock: "No" at the exit puts the snapshot back
{
    int k;
    for (k = 0; k < 2; ++k)
        if (*RzlSetting(k) != g_rzlSnap[k]) RzlSetLocked(k, g_rzlSnap[k]);
}

// ---------------------------------------------------------------- the rows
static int g_rzlRow[2] = { -1, -1 };
static int RzoAvail(void) { return !g_rzoFail && eo_api[0] == 0x32564F45u; }  // the outfit engine is installed ('EOV2')
static int RzgAvail(void) { return RzAvail() && !g_rzgFail; }                  // the room-load hook (enemies.inc EnmWrap)
static int RzlStep(int k, int avail)
{
    int v;
    if (!avail) {
        if (((Fn2)0x4F84A0)(0, 0x11) || (((Fn1)0x5542C0)(0) & 0x0C)) ((Fn1)0x56CC40)(0x9C44);
        return 0;
    }
    GpLock();
    v = StepValue((int)*RzlSetting(k), 2);
    if (v >= 0) RzlSetLocked(k, v);
    GpUnlock();
    return 0;
}
static int __cdecl RzlInOutfit(BYTE *cfg, int count) { (void)cfg; (void)count; return RzlStep(0, RzoAvail()); }
static int __cdecl RzlInGhosts(BYTE *cfg, int count) { (void)cfg; (void)count; return RzlStep(1, RzgAvail()); }
static int __cdecl RzlGetOutfit(void) { return g_rzoMode ? 1 : 0; }
static int __cdecl RzlGetGhosts(void) { return g_rzgMode ? 1 : 0; }
static int __cdecl RzlDescOutfit(void) { return !RzoAvail() ? MSG_RZ0 + RZM_NA : MSG_RZL0 + RZLM_DO0 + RzlGetOutfit(); }
static int __cdecl RzlDescGhosts(void) { return !RzgAvail() ? MSG_RZ0 + RZM_NA : MSG_RZL0 + RZLM_DG0 + RzlGetGhosts(); }
static int RzlBuildRows(DWORD *t, int n)  // randomizer.inc RzBuildPage: Eileen Outfit, Ghosts at row n; the next free row
{
    g_rzlRow[0] = n;
    FillRow(t + 7 * n++, RzlInOutfit, RzlGetOutfit, g_rzoIds, 2, MSG_RZL0 + RZLM_LO, RzlDescOutfit);
    g_rzlRow[1] = n;
    FillRow(t + 7 * n++, RzlInGhosts, RzlGetGhosts, g_rzgIds, 2, MSG_RZL0 + RZLM_LG, RzlDescGhosts);
    return n;
}
static int RzlRowChanged(int row)  // randomizer.inc RzRowChanged (the yellow mark)
{
    int k;
    if (!g_snapOn || row < 0) return 0;
    for (k = 0; k < 2; ++k)
        if (row == g_rzlRow[k]) return *RzlSetting(k) != g_rzlSnap[k];
    return 0;
}

// ---------------------------------------------------------------- Eileen Outfit (eileenoutfit.inc eo_stage_c / eo_room_c)
static const char *const kRzoWhere[2] = { "her world's file list", "a room load" };
static void RzlOutfitStep(int atWorld)
{
    const int stage = (int)*(volatile DWORD *)0xFD5A90, half = (*(volatile const BYTE *)0x13D812D & 4) ? 1 : 0;
    const int room = (int)*(volatile DWORD *)0xFD5A94;  // v3.50 RL350: gGame.room (4F3510 loads gGame.stage / room's list)
    int put = -2, ev, here, was = -2;
    unsigned took;
    if (g_rzoFail) return;
    __try {
        here = atWorld || (eo_scan() & 0x33) != 0;  // the world's file list holds eil.bin / her models are in the file cache
        took = g_rzo.takeovers;
        was = eo_outfit;
        ev = RzoStep(&g_rzo, (int)g_rzoMode, (unsigned)g_rzSeed, RZ_PERVISIT_ON(), stage, half, here, RZO_ALL, eo_outfit,  // v3.56 RI356: By Room = per room
                     g_eoSaved, atWorld, room, &put);  // v3.50 RL350: every outfit (was RzoUnlocked(the Nurse's flag)), + room loads
        if (put >= -1 && put <= 1) eo_outfit = put;  // (the engine's tick, stage and room hooks take it from here)
        if (g_rzo.takeovers != took && ev != RZO_EV_TAKEOVER && LogQuota(&g_rzoQuota, 400))  // (a choice, then straight on)
            Log("randomizer: outfit: %s chosen in the Extra Menu: kept until the next world visit", RzoOutfitName(was));
        if (ev == RZO_EV_ROOM) {  // v3.50 RL350: a room load's draw (the room's spawn builds her in it)
            if (LogQuota(&g_rzoQuotaRoom, 600))
                Log("randomizer: outfit: room %d:%d load (world visit %u, room draw %u): %s -> %s (seed %lu)", stage, room, g_rzo.visit,
                    g_rzo.seq, RzoOutfitName(was), RzoOutfitName(put), (unsigned long)(unsigned)g_rzSeed);
            return;
        }
        if (ev == RZO_EV_HELD) {  // v3.50 RL350
            if (LogQuota(&g_rzoQuotaRoom, 600))
                Log("randomizer: outfit: room %d:%d load: no room draw in the final fight (stage %d): her outfit stays %s for this "
                    "visit", stage, room, RZO_FINAL_STAGE, RzoOutfitName(eo_outfit));
            return;
        }
        if (ev == RZO_EV_NONE || !LogQuota(&g_rzoQuota, 400)) return;
        if (ev == RZO_EV_DRAW)
            Log("randomizer: outfit: world visit %u (stage %d, half %d, at %s): %s (seed %lu, Change Each Visit %s; every outfit: "
                "Dress, Nurse; the set outfit %s)", g_rzo.visit, stage, half + 1, kRzoWhere[atWorld ? 0 : 1], RzoOutfitName(put),
                (unsigned long)(unsigned)g_rzSeed, kRzKeepName[RZ_PERVISIT_ON() ? 1 : 0], RzoOutfitName(g_eoSaved));  // v3.50 RL350: was "unlocked: Dress, Nurse"  // v3.56 RI356: On = Reroll By Room
        else if (ev == RZO_EV_SINGLE)  // (v3.50 RL350: no longer with RZO_ALL)
            Log("randomizer: outfit: world visit %u (stage %d, half %d, at %s): only the Dress is unlocked in this save: her "
                "outfit stays %s", g_rzo.visit, stage, half + 1, kRzoWhere[atWorld ? 0 : 1], RzoOutfitName(eo_outfit));
        else if (ev == RZO_EV_RESTORE)
            Log("randomizer: outfit: back to the set outfit %s (%s)", RzoOutfitName(put),
                g_rzoMode != RZO_RANDOM ? "Eileen Outfit Off" : "a world without her");
        else if (ev == RZO_EV_TAKEOVER)
            Log("randomizer: outfit: %s chosen in the Extra Menu: kept until the next world visit", RzoOutfitName(eo_outfit));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_rzoFail = 1;
        LogAnomaly("randomizer", "outfit: faulted at %s (code %08lX): Eileen Outfit is off for this session (her outfit as set)",
                   kRzoWhere[atWorld ? 0 : 1], GetExceptionCode());
    }
}
static void RzlOutfitWorld(void) { RzlOutfitStep(1); }  // eo_stage_c: the world's file list holds eil.bin
static void RzlOutfitRoom(void) { RzlOutfitStep(0); }   // eo_room_c: every room load (not the neighbour preload)

// ---------------------------------------------------------------- Ghosts (randomizer.inc RzBeforeScript, level d)
static int RzlSpotTest(void *ctx, const float *base, float *cand)  // rzlooks_sim.h RzgTestFn
{
    return EnmSpotTest(base, cand, 2, *(const float *)ctx) == ENM_SPOT_OK;
}
static int RzlFloorAt(const float *pos, float *y)  // EnemyFloorRayCheck 0x459490 from just above the point (as Copies' flyers)
{
    __declspec(align(16)) float p[4];
    __declspec(align(16)) EnmFloorHit hit;
    memset(&hit, 0, sizeof hit);
    p[0] = pos[0], p[1] = pos[1] - 4.0f, p[2] = pos[2], p[3] = 1.0f;
    if (((int(__cdecl *)(EnmFloorHit *, float *))0x459490)(&hit, p) != 1 || !_finite(hit.pos[1])) return 0;
    *y = hit.pos[1];
    return 1;
}
static int RzlCt7700(void)  // the cheat table's Enemy Randomizer took the two `call 52C900` sites of the runner
{
    return memcmp((const void *)0x52CAA9, "\xE8\x52\xFE\xFF\xFF", 5) || memcmp((const void *)0x52CB2B, "\xE8\xD0\xFD\xFF\xFF", 5);
}
// the spot test's game functions (enemies.inc EnmSpotTest; Copies call them too): stock, Refined's own hook or Playable
// Eileen's listed hook, else Ghosts stay off (test/rzlooks_sim_test.c checks these bytes on the stock exe)
typedef struct { DWORD va; int len; BYTE b[12]; } RzlFn;
static const RzlFn kRzlFns[6] = {  // v3.50 RL350: + the record position functions (was 4)
    { 0x459490, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x34 } },                    // EnemyFloorRayCheck
    { 0x458F00, 12, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0x84, 0x00, 0x00, 0x00 } },  // room to stand
    { 0x46F700, 10, { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x1D, 0x90, 0x5A, 0xFD, 0x00 } },              // a point inside geometry
    { 0x454000, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x28 } },                    // line of sight
    { 0x46FE30, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x10 } },                    // v3.50 RL350: world -> record
    { 0x46FE80, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x10 } },                    // v3.50 RL350: record -> world
};
static int RzlFnsOk(void)
{
    int i, ok = 1;
    for (i = 0; i < (int)(sizeof kRzlFns / sizeof kRzlFns[0]); ++i) {  // v3.50 RL350 (was 4)
        const RzlFn *f = &kRzlFns[i];
        if (!memcmp((const void *)(ULONG_PTR)f->va, f->b, (size_t)f->len) || EmOurPatch(f->va, f->len) ||
            PeHookedCallee(f->va, "randomizer: ghosts"))
            continue;
        Log("randomizer: ghosts: %08lX (the spot test's game code) is not the stock code: Ghosts are off for this session", f->va);
        ok = 0;
    }
    return ok;
}
static void RzlFirstLoad(int stage, int room)
{
    const HMODULE ds = GetModuleHandleA("dsound.dll");
    const int pe = ds && HasExport(ds, "SH4PE_Patches");  // Playable Eileen (it may load after this dll)
    Log("randomizer: looks: the first room load (stage %d room %d): Eileen Outfit %s, %s; Spawn Site %s, %s", stage, room,  // v3.56 RE356
        kRzoName[g_rzoMode ? 1 : 0],
        !RzoAvail() ? "NOT available: the outfit engine is off ([EileenOutfit] Enabled 0, or its sites are not stock)"
        : pe ? "the companion only: Playable Eileen dresses the player ([Variant] in SH4PlayableEileen.ini)"
                     : "companion Eileen",
        kRzgName[g_rzgMode ? 1 : 0],
        !RzAvail() ? "NOT available: no room-load hook" : g_rzgFail ? "NOT available: the spot test's game code is not stock" : RzlCt7700() ? "the cheat table's Enemy Randomizer is on: enemies left alone"
                                                                       : "monsters and victim ghosts, never the story ghosts");  // v3.56 RE356
}
// v3.50 RL350: the live records of a mover's id (the game's 64): what Ghosts does with them (RzgRecRule, the strongest of
// them). move: every standing record of it in this room is put at `spot` (world -> record, 0x46FE30; its w kept).
// keptAt (may be NULL): the world spot of a pinned / down record in this room (it starts there)
static int RzlGhostRec(unsigned id, int stage, int room, const float *spot, int move, float *keptAt)
{
    BYTE *r = ENM_RECORDS;
    int i, how = RZG_REC_NONE;
    for (i = 0; i < 64; ++i, r += 0x50) {
        const int st = *(const short *)(r + 0x26);
        int rule;
        if (!st || *(const unsigned *)(r + 0x20) != id) continue;
        rule = RzgRecRule(st, *(const float *)(r + 0x3C), (signed char)r[0x36], (signed char)r[0x37], stage, room);
        if (rule == RZG_REC_MOVE && move && spot) {
            __declspec(align(16)) float in[4], out[4];
            in[0] = spot[0], in[1] = spot[1], in[2] = spot[2], in[3] = *(const float *)(r + 0x0C);
            ENM_TO_REC(out, in);
            if (RzgFinite3(out)) memcpy(r, out, 12);
            else rule = RZG_REC_NONE;  // (never with a finite origin: the record is left as it is)
        }
        if ((rule == RZG_REC_PINNED || rule == RZG_REC_DOWN) && keptAt) {
            __declspec(align(16)) float in[4], out[4];
            memcpy(in, r, 16);
            ENM_FROM_REC(out, in);
            memcpy(keptAt, out, 12);
        }
        how = RzgRecWorse(how, rule);
    }
    return how;
}
static void RzlGhostsBeforeC(int room, int d)
{
    const int stage = ENM_STAGE, half = (*(volatile const BYTE *)0x13D812D & 4) ? 1 : 0;
    const unsigned seed = (unsigned)g_rzSeed;
    RzgRoom R;
    float pp[3], taken[RZG_MOVERS_MAX][3], base[3], out[3], floor0;
    float ghosts[RZG_SPOTS_MAX + RZG_MOVERS_MAX][3];  // v3.50 RL350: the ghosts' placements + pinned / down records' spots
    int recHow[RZG_MOVERS_MAX];                       // v3.50 RL350: RzgRecRule of each mover before the move
    char list[1000];
    int i, k, o = 0, moved = 0, kept = 0, left = 0, nTaken = 0, nGhosts = 0;
    unsigned visit, start;
    if (!g_rzlFirstSaid++) {
        if (!RzlFnsOk()) g_rzgFail = 1;
        RzlFirstLoad(stage, room);
        if (g_rzgFail) return;
    }
    if (g_rzgMode != RZG_SPOTS || stage < 2 || stage > 8 || room <= 0 || room >= RZ_ROOMS) return;
    if (RzlCt7700()) {
        if (!g_rzgCtSaid++)
            Log("randomizer: ghosts: the cheat table's Enemy Randomizer (0052CAA9 / 0052CB2B) is on: the ghosts start where the "
                "game puts them");
        return;
    }
    start = ((const DWORD *)0x10A0A78)[room];
    if (!start) return;
    if (!RzgScanRoom(RzWindow(), start, &R)) {
        if (LogQuota(&g_rzgQuotaAnom, 20))
            LogAnomaly("randomizer", "ghosts: room %d:%d: its script (%08X) could not be read: its ghosts are left alone", stage,
                       room, start);
        return;
    }
    if (!R.nm) return;
    ++g_rzgRooms;
    g_rzgVictims += R.nm;
    EnmPlayerPos(pp, 1);
    visit = RzVisitSalt(stage, room);  // v3.56 RI356: By Room = the room's entries, By World = the world visit (worldvisit.inc)
    list[0] = 0;
    for (i = 0; i < R.nghost; ++i) memcpy(ghosts[nGhosts++], R.ghost[i], 12);  // v3.50 RL350
    for (i = 0; i < R.nm; ++i) {  // v3.50 RL350: a pinned / down record in this room keeps its ghost there: an occupied spot
        float at[3];
        recHow[i] = RzlGhostRec(R.m[i].id, stage, room, NULL, 0, at);
        if ((recHow[i] == RZG_REC_PINNED || recHow[i] == RZG_REC_DOWN) && RzgFinite3(at) && nGhosts < (int)(sizeof ghosts / sizeof ghosts[0]))
            memcpy(ghosts[nGhosts++], at, 12);
    }
    for (i = 0; i < R.nm; ++i) {
        const RzgMover *m = &R.m[i];
        RzgPickIn in;
        int nv = 0, nt = 0, n0, near1 = 0, rec, boxed = 0;  // v3.56 RE356: + boxed
        for (k = 0; k < 3; ++k) base[k] = RzgF(m->pos[k]);
        if (o < (int)sizeof list - 160)
            o += sprintf_s(list + o, sizeof list - o, "%s%08X %02X v%u %.0f %.0f %.0f", i ? "; " : "", m->id, m->type,
                           (m->mode >> 8) & 0xF, base[0], base[1], base[2]);
        if (recHow[i] == RZG_REC_PINNED || recHow[i] == RZG_REC_DOWN) {  // v3.50 RL350: the record wins: where it lies
            ++kept, ++g_rzgRecKept;
            if (o < (int)sizeof list - 60) o += sprintf_s(list + o, sizeof list - o, " kept (its record: %s)", kRzgRecName[recHow[i]]);
            continue;
        }
        if (!RzlFloorAt(base, &floor0)) {
            ++kept;
            if (o < (int)sizeof list - 60) o += sprintf_s(list + o, sizeof list - o, " kept (no floor under it)");
            continue;
        }
        in.base = base, in.key = RzgKey(seed, stage, half, room, m->n, visit);
        in.stock = (const float(*)[3])R.spot, in.nStock = R.nspot;
        in.player = pp, in.taken = (const float(*)[3])taken, in.nTaken = nTaken;
        in.ghosts = (const float(*)[3])ghosts, in.nGhosts = nGhosts;  // v3.50 RL350
        {  // v3.56 RE356: Spawn Site: anywhere in the room's box in x / y / z (spawnsite.inc SsGhostSite); else v3.50's rings
            int sw[SSW_N], j;
            memset(sw, 0, sizeof sw);
            boxed = SsGhostSite(stage, half, room, m->n, 1, base, floor0 - base[1], (const float(*)[3])ghosts, nGhosts, out, sw);
            if (boxed) {
                for (j = 0, nt = 1; j < SSW_N; ++j) nt += sw[j];
                nv = 1, near1 = 0;
            }
        }
        if (!boxed && !RzgPick2(&in, RzlSpotTest, &floor0, out, &nv, &nt, &near1)) {
            ++kept;
            if (o < (int)sizeof list - 60) o += sprintf_s(list + o, sizeof list - o, " kept (no spot fits, 0 of %d)", nt);
            continue;
        }
        n0 = g_rzStack.n[d];
        for (k = 0; k < 3; ++k)
            if (!RzAddPatch(d, m->posVa + 4u * (unsigned)k, m->pos[k], RzgU(out[k]))) break;
        if (k < 3) {
            RzStackTrim(&g_rzStack, d, n0);  // (the words written for it back)
            ++left;
            if (o < (int)sizeof list - 60) o += sprintf_s(list + o, sizeof list - o, " kept (its words are not the stock ones)");
            continue;
        }
        taken[nTaken][0] = out[0], taken[nTaken][1] = out[1], taken[nTaken][2] = out[2];
        if (!boxed) SsTake(out);  // v3.56 RE356: (a box site is taken by SsGhostSite)
        ++nTaken, ++moved;
        rec = recHow[i] == RZG_REC_MOVE ? RzlGhostRec(m->id, stage, room, out, 1, NULL) : recHow[i];  // v3.50 RL350
        g_rzgRecMoved += rec == RZG_REC_MOVE;
        g_rzgFar += !near1;
        if (o < (int)sizeof list - 100)
            o += sprintf_s(list + o, sizeof list - o, " -> %.0f %.0f %.0f (d %.0f%s, %d of %d fit, record %s)", out[0], out[1], out[2],
                           sqrtf(RzgDist2(out, base)), near1 ? " near: the room allows no farther" : "", nv, nt, kRzgRecName[rec]);
    }
    g_rzgMoved += moved, g_rzgKept += kept, g_rzgLeft += left;
    if (LogQuota(&g_rzgQuota, 4000))
        Log("randomizer: ghosts: room %d:%d (#%u): %d of %d victim(s) moved (seed %lu, Change Each Visit %s): %s", stage, room, visit,
            moved, R.nm, (unsigned long)seed, kRzKeepName[RZ_PERVISIT_ON() ? 1 : 0], list);  // v3.56 RI356: On = Reroll By Room
    if (left && LogQuota(&g_rzgQuotaAnom, 20))
        LogAnomaly("randomizer", "ghosts: room %d:%d: %d victim(s) whose position words did not hold the stock value (another "
                   "mod?): left as found", stage, room, left);
}
static void RzlGhostsBefore(int room, int d)  // after the Items' words of this run (same level d; RzAfterScript puts all back)
{
    int n0;
    if (g_rzFail || g_rzgFail || d < 0 || d >= RZ_DEPTH) return;
    n0 = g_rzStack.n[d];
    __try {
        RzlGhostsBeforeC(room, d);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        int left = 0;
        __try {
            left = RzStackTrim(&g_rzStack, d, n0);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
        }
        g_rzgFail = 1;
        LogAnomaly("randomizer", "ghosts: faulted at a room load (code %08lX; %d word(s) not restored): Ghosts are off for this "
                   "session", GetExceptionCode(), left);
    }
}

// ---------------------------------------------------------------- the session line (randomizer.inc RzSessionLine)
static void RzlSessionLine(void)  // v3.50 RL350: + room draws, moves of 800 +, records moved / kept (was "with only the Dress unlocked")
{
    Log("randomizer: looks session: Eileen Outfit %s: %u world visit(s), %u world draw(s) (all draws: Dress %u, Nurse %u), %u room "
        "draw(s) (%u changed her outfit), %u back to the set outfit, %u chosen in the Extra Menu%s; Spawn Site %s: %d room load(s) with victims, %d "
        "victim(s), %d moved (%d of them 800 or more, %d record(s) moved), %d kept (%d by a pinned or down record), %d left as "
        "found%s",
        kRzoName[g_rzoMode ? 1 : 0], g_rzo.visit, g_rzo.draws, g_rzo.drawn2[0], g_rzo.drawn2[1], g_rzo.roomDraws, g_rzo.roomChanged,
        g_rzo.restores, g_rzo.takeovers, g_rzoFail ? "; FAULTED (off)" : !RzoAvail() ? "; no outfit engine (off)" : "",
        kRzgName[g_rzgMode ? 1 : 0], g_rzgRooms, g_rzgVictims, g_rzgMoved, g_rzgFar, g_rzgRecMoved, g_rzgKept, g_rzgRecKept, g_rzgLeft,
        g_rzgFail ? "; FAULTED (off)" : !RzgAvail() ? "; no room-load hook (off)" : "");
    SsSessionLine();  // v3.56 RE356
}
