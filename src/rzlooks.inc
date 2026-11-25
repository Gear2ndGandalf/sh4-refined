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

#include "rzlooks_sim.h"

static volatile LONG g_rzoMode = RZO_DEFAULT, g_rzgMode = RZG_DEFAULT;
static LONG g_rzlSnap[2];        // as Options opened
static int g_rzlUnsaved;         // the menu's value could not be written (the poll leaves the file's value alone)
static int g_rzoFail, g_rzgFail;  // faulted: that half is off for this session
static RzoState g_rzo;
static int g_rzoChecked, g_rzoNurseOk;  // SaveGetNurseUnlocked 0x56F590 is the stock code (its flag 0x13D1A50 is read)
static int g_rzlFirstSaid, g_rzgCtSaid;
static int g_rzgRooms, g_rzgMoved, g_rzgKept, g_rzgLeft, g_rzgVictims;
static volatile LONG g_rzoQuota, g_rzgQuota, g_rzgQuotaAnom;
static const BYTE kRzoNurseFn[6] = { 0xA1, 0x50, 0x1A, 0x3D, 0x01, 0xC3 };  // 0x56F590: mov eax,[013D1A50] / ret

// ---------------------------------------------------------------- texts (MSG_RZ1 + 16 .. + 31: this worker's half)
#define MSG_RZL0 (MSG_RZ1 + 16)
enum { RZLM_LO, RZLM_VR, RZLM_DO0, RZLM_DO1, RZLM_LG, RZLM_VS, RZLM_DG0, RZLM_DG1, RZLM_N };
typedef char RzlTextsFit[MSG_RZL0 + RZLM_N <= MSG_END ? 1 : -1];
typedef char RzlRowsFit[(6 + 2 + 1) * 7 + 1 <= sizeof g_rowsRando / sizeof g_rowsRando[0] ? 1 : -1];  // + w3's Monsters: 10
static BYTE g_txtRzl[RZLM_N][64];
static DWORD g_rzoIds[2], g_rzgIds[2];
static const BYTE *RzlText(int id)
{
    return id >= MSG_RZL0 && id < MSG_RZL0 + RZLM_N ? g_txtRzl[id - MSG_RZL0] : RzText(MSG_RZ0 + RZM_NA);
}
static void RzlTexts(void)  // randomizer.inc RzTexts
{
    Encode(g_txtRzl[RZLM_LO], 64, "Eileen Outfit");
    Encode(g_txtRzl[RZLM_VR], 64, "Random");
    Encode(g_txtRzl[RZLM_DO0], 64, "Off: companion Eileen's outfit as set.");  // (widths: 331 / 392 / 396 / 380 px of 416)
    Encode(g_txtRzl[RZLM_DO1], 64, "Random: an unlocked outfit each world visit.");
    Encode(g_txtRzl[RZLM_LG], 64, "Ghosts");
    Encode(g_txtRzl[RZLM_VS], 64, "Spots");
    Encode(g_txtRzl[RZLM_DG0], 64, "Off: victims start where the game puts them.");  // (<= 405 px: tools/desc_measure.py)
    Encode(g_txtRzl[RZLM_DG1], 64, "Spots: a new spot in their room. Next door.");
    g_rzoIds[0] = g_rzgIds[0] = MSG_RZ0 + RZM_VK0;  // "Off" (the Keep Specials row's)
    g_rzoIds[1] = MSG_RZL0 + RZLM_VR;
    g_rzgIds[1] = MSG_RZL0 + RZLM_VS;
}

// ---------------------------------------------------------------- [Randomizer] EileenOutfit / Ghosts
static const char *const kRzlKey[2] = { "EileenOutfit", "Ghosts" };
static volatile LONG *RzlSetting(int k) { return k ? &g_rzgMode : &g_rzoMode; }
static const char *RzlValName(int k, LONG v) { return k ? kRzgName[v ? 1 : 0] : kRzoName[v ? 1 : 0]; }
static void RzlReadIni(void)  // randomizer.inc RzReadIni (RzInit): the keys and the start line
{
    g_rzoMode = RzReadKey("EileenOutfit", RzoOfText, RZO_DEFAULT, kRzoName, "Off / Random");
    g_rzgMode = RzReadKey("Ghosts", RzgOfText, RZG_DEFAULT, kRzgName, "Off / Spots");
    RzoInit(&g_rzo);
    Log("randomizer: looks: [Randomizer] EileenOutfit %s, Ghosts %s (companion Eileen's outfit once per world visit among the "
        "unlocked ones; victim ghosts at another spot of their room: patch-and-restore of their position words, no exe patch)",
        kRzoName[g_rzoMode ? 1 : 0], kRzgName[g_rzgMode ? 1 : 0]);
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
    int put = -2, ev, here, nurse, was = -2;
    unsigned took;
    if (g_rzoFail) return;
    __try {
        if (!g_rzoChecked) {
            g_rzoChecked = 1;
            g_rzoNurseOk = !memcmp((const void *)0x56F590, kRzoNurseFn, sizeof kRzoNurseFn);
            if (!g_rzoNurseOk)
                LogAnomaly("randomizer", "outfit: SaveGetNurseUnlocked 0056F590 is not the stock code: the Nurse counts as locked "
                           "(Eileen Outfit Random keeps her outfit as set)");
        }
        nurse = g_rzoNurseOk && *(volatile const int *)0x13D1A50 != 0;
        here = atWorld || (eo_scan() & 0x33) != 0;  // the world's file list holds eil.bin / her models are in the file cache
        took = g_rzo.takeovers;
        was = eo_outfit;
        ev = RzoStep(&g_rzo, (int)g_rzoMode, (unsigned)g_rzSeed, g_rzPerVisit != 0, stage, half, here, RzoUnlocked(nurse),
                     eo_outfit, g_eoSaved, &put);
        if (put >= -1 && put <= 1) eo_outfit = put;  // (the engine's tick, stage and room hooks take it from here)
        if (g_rzo.takeovers != took && ev != RZO_EV_TAKEOVER && LogQuota(&g_rzoQuota, 400))  // (a choice, then straight on)
            Log("randomizer: outfit: %s chosen in the Extra Menu: kept until the next world visit", RzoOutfitName(was));
        if (ev == RZO_EV_NONE || !LogQuota(&g_rzoQuota, 400)) return;
        if (ev == RZO_EV_DRAW)
            Log("randomizer: outfit: world visit %u (stage %d, half %d, at %s): %s (seed %lu, Change Each Visit %s; unlocked: Dress, "
                "Nurse; the set outfit %s)", g_rzo.visit, stage, half + 1, kRzoWhere[atWorld ? 0 : 1], RzoOutfitName(put),
                (unsigned long)(unsigned)g_rzSeed, kRzKeepName[g_rzPerVisit ? 1 : 0], RzoOutfitName(g_eoSaved));
        else if (ev == RZO_EV_SINGLE)
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
static const RzlFn kRzlFns[4] = {
    { 0x459490, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x34 } },                    // EnemyFloorRayCheck
    { 0x458F00, 12, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0x84, 0x00, 0x00, 0x00 } },  // room to stand
    { 0x46F700, 10, { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x1D, 0x90, 0x5A, 0xFD, 0x00 } },              // a point inside geometry
    { 0x454000, 9, { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x28 } },                    // line of sight
};
static int RzlFnsOk(void)
{
    int i, ok = 1;
    for (i = 0; i < 4; ++i) {
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
    Log("randomizer: looks: the first room load (stage %d room %d): Eileen Outfit %s, %s; Ghosts %s, %s", stage, room,
        kRzoName[g_rzoMode ? 1 : 0],
        !RzoAvail() ? "NOT available: the outfit engine is off ([EileenOutfit] Enabled 0, or its sites are not stock)"
        : pe ? "the companion only: Playable Eileen dresses the player ([Variant] in SH4PlayableEileen.ini)"
                     : "companion Eileen",
        kRzgName[g_rzgMode ? 1 : 0],
        !RzAvail() ? "NOT available: no room-load hook" : g_rzgFail ? "NOT available: the spot test's game code is not stock" : RzlCt7700() ? "the cheat table's Enemy Randomizer is on: ghosts left alone"
                                                                       : "victim ghosts, never the story ghosts");
}
static void RzlGhostsBeforeC(int room, int d)
{
    const int stage = ENM_STAGE, half = (*(volatile const BYTE *)0x13D812D & 4) ? 1 : 0;
    const unsigned seed = (unsigned)g_rzSeed;
    RzgRoom R;
    float pp[3], taken[RZG_MOVERS_MAX][3], base[3], out[3], floor0;
    char list[600];
    int i, k, o = 0, moved = 0, kept = 0, left = 0, nTaken = 0;
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
    visit = g_rzPerVisit ? g_rzRoomVisit[stage][room] : 0;
    list[0] = 0;
    for (i = 0; i < R.nm; ++i) {
        const RzgMover *m = &R.m[i];
        RzgPickIn in;
        int nv = 0, nt = 0, n0;
        for (k = 0; k < 3; ++k) base[k] = RzgF(m->pos[k]);
        if (o < (int)sizeof list - 120)
            o += sprintf_s(list + o, sizeof list - o, "%s%08X %02X v%u %.0f %.0f %.0f", i ? "; " : "", m->id, m->type,
                           (m->mode >> 8) & 0xF, base[0], base[1], base[2]);
        if (!RzlFloorAt(base, &floor0)) {
            ++kept;
            if (o < (int)sizeof list - 60) o += sprintf_s(list + o, sizeof list - o, " kept (no floor under it)");
            continue;
        }
        in.base = base, in.key = RzgKey(seed, stage, half, room, m->n, visit);
        in.stock = (const float(*)[3])R.spot, in.nStock = R.nspot;
        in.player = pp, in.taken = (const float(*)[3])taken, in.nTaken = nTaken;
        if (!RzgPick(&in, RzlSpotTest, &floor0, out, &nv, &nt)) {
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
        ++nTaken, ++moved;
        if (o < (int)sizeof list - 60)
            o += sprintf_s(list + o, sizeof list - o, " -> %.0f %.0f %.0f (%d of %d fit)", out[0], out[1], out[2], nv, nt);
    }
    g_rzgMoved += moved, g_rzgKept += kept, g_rzgLeft += left;
    if (LogQuota(&g_rzgQuota, 4000))
        Log("randomizer: ghosts: room %d:%d (#%u): %d of %d victim(s) moved (seed %lu, Change Each Visit %s): %s", stage, room, visit,
            moved, R.nm, (unsigned long)seed, kRzKeepName[g_rzPerVisit ? 1 : 0], list);
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
static void RzlSessionLine(void)
{
    Log("randomizer: looks session: Eileen Outfit %s: %u world visit(s), %u draw(s) (Dress %u, Nurse %u), %u with only the "
        "Dress unlocked, %u back to the set outfit, %u chosen in the Extra Menu%s; Ghosts %s: %d room load(s) with victims, %d "
        "victim(s), %d moved, %d kept, %d left as found%s",
        kRzoName[g_rzoMode ? 1 : 0], g_rzo.visit, g_rzo.draws, g_rzo.drawn2[0], g_rzo.drawn2[1], g_rzo.single, g_rzo.restores,
        g_rzo.takeovers, g_rzoFail ? "; FAULTED (off)" : !RzoAvail() ? "; no outfit engine (off)" : "", kRzgName[g_rzgMode ? 1 : 0],
        g_rzgRooms, g_rzgVictims, g_rzgMoved, g_rzgKept, g_rzgLeft,
        g_rzgFail ? "; FAULTED (off)" : !RzgAvail() ? "; no room-load hook (off)" : "");
}
