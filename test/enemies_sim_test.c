// v3.58 EN358: + the randomizer's victims take the ghosts' path, the per-door ghost summary.
// v3.38 EN338: + no timing rule, the pursuit for the world visit, spotted at any distance, story ghosts follow, no
// Wheelchair, the space gates, the game's own downs.
// v3.35 GH335: + victims pursue (mode 4), victims' copies follow, late copies, the down countdown at 30-180 Hz.
// v3.34 EN334: + released event dogs revive, copies follow, the copies' ledger, wanderer spots.
// v3.31 EN331: + story ghosts, ghost copies beside a downed original, the record decay and clock at 30-180 Hz, the store, flyers.
// v3.30 EN330: + the pool at a room change, follow modes, the door verdict, kill bits, Revive Enemies, spot tiers, the queue at 30-180 Hz.
// v3.31 ENEMIES: native test of src/enemies_sim.h (Enemy Copies / Enemies Follow pure rules): copy ids and the purge
// rule, eligible types, the bone budget at x2 / x3 / x4 with the stock and the Auto pool, the round robin, the pool size,
// the dead ring, pursuer picking, the door rule, the follower queue timing, wanderers, the spot ring.
// Build and run: test\build_enemies_test.cmd (cl /O2 x86, as build.cmd).
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "../src/enemies_sim.h"

static int g_fail, g_n;
static void Check(int ok, const char *what)
{
    ++g_n;
    if (!ok) {
        printf("FAIL  %s\n", what);
        ++g_fail;
    }
}

static void TestIds(void)
{
    static unsigned char seen[1 << 16];
    int room, n, k, dup = 0, bad = 0;
    for (room = 1; room < ENM_COPY_ROOMS; ++room)
        for (n = 0; n < ENM_COPY_NS; ++n)
            for (k = 1; k <= ENM_COPY_MAXK; ++k) {
                const unsigned id = EnmCopyId(room, n, k);
                if (!EnmCopyIdOk(room, n, k)) ++bad;
                if (seen[id & 0xFFFF]++) ++dup;
                if (!EnmIsCopyId(id) || EnmIsScriptId(id) || EnmIsGhostId(id) || (id >> 16 & 0xFF) != 0) ++bad;
                if (EnmScriptStage(id) == 0) ++bad;  // the automatic id 0x463AA0 counts only stage-0 ids
                if ((int)(id >> 10 & 0x3F) != room || (int)(id >> 2 & 0xFF) != n || (int)(id & 3) != k) ++bad;
            }
    Check(!dup, "copy ids: unique over rooms 1..63, n 0..255, k 1..3");
    Check(!bad, "copy ids: never a script id, never FFFF00nn, bits 16..23 zero, stage bits set, fields round-trip");
    Check(!EnmCopyIdOk(64, 0, 1) && !EnmCopyIdOk(0, 0, 1) && !EnmCopyIdOk(5, 256, 1) && !EnmCopyIdOk(5, 3, 0) &&
              !EnmCopyIdOk(5, 3, 4), "copy ids: out of the packing -> no copy (the stock scripts use rooms <= 42, n <= 30)");
    // the purge rule: only ours, never the table's 60 / 61 / 1F00 ids, script ids or ghost ids
    Check(!EnmIsCopyId(0x60123456u) && !EnmIsCopyId(0x61003421u) && !EnmIsCopyId(0x1F000005u) && !EnmIsCopyId(0x1F00FFFFu),
          "purge: the table's extras (60), victim copies (61) and Spawn Enemies (1F00) are not ours");
    Check(!EnmIsCopyId(0x1E050003u), "purge: a 1E id with a room byte (a script id of stage 0x1E) is not ours");
    Check(!EnmIsCopyId(EnmScriptId(5, 0, 7, 3)) && !EnmIsCopyId(0xFFFF000Au), "purge: script and victim ids are kept");
    {
        const unsigned s = EnmScriptId(5, 1, 12, 7);
        Check(s == 0x850C0007u && EnmScriptStage(s) == 5 && EnmScriptRoom(s) == 12 && EnmScriptN(s) == 7 && EnmScriptHalf(s) == 1 &&
                  EnmIsScriptId(s), "script id: stage << 24 | room << 16 | n, + 0x80000000 second half (0x52CA00)");
        Check(!EnmIsScriptId(0x05000007u) && !EnmIsScriptId(0x000C0007u), "script id: needs a stage and a room (0x52C8D0)");
    }
    Check(EnmCopyKey(3, 0, EnmCopyId(5, 2, 1)) != EnmCopyKey(4, 0, EnmCopyId(5, 2, 1)) &&
              EnmCopyKey(3, 0, EnmCopyId(5, 2, 1)) != EnmCopyKey(3, 1, EnmCopyId(5, 2, 1)),
          "dead key: the same copy id in another stage / half is another copy");
}

static void TestTypes(void)
{
    const unsigned clr = 0;
    int t, bad = 0;
    for (t = 0; t < 0x20; ++t) {
        const int mon = EnmCopyEligible(t, 0, 0, clr, clr);
        const int want = t == 2 || t == 3 || t == 7 || t == 8 || t == 0x0A || t == 0x11;  // v3.38 EN338: no 06 Wheelchair
        if (mon != want) ++bad;
    }
    Check(!bad, "copies: monsters 02 03 07 08 0A 11 only (no 01, 04 mm, 05 Wall Man, 06 Wheelchair, 09 Tremer, 10 Walter, 12+)");
    Check(!EnmCopyEligible(3, 0, 6, clr, clr) && !EnmCopyEligible(3, 0, 7, clr, clr) && !EnmCopyEligible(5, 1, 8, clr, clr),
          "copies: event / boss spawn modes are never copied");
    Check(!EnmCopyEligible(3, 0, 0, 0xFFFF0003u, clr) && !EnmCopyEligible(3, 0, 0, 0x12345678u, clr),
          "copies: a monster with an id override is not copied");
    Check(EnmCopyEligible(0x0B, 0, 0, 0xFFFF000Au, clr) && EnmCopyEligible(0x0B, 3, 0, 0xFFFF0007u, clr) &&
              EnmCopyEligible(0x0C, 1, 0, 0xFFFF0008u, clr) && EnmCopyEligible(0x0D, 1, 0, 0xFFFF0011u, clr),
          "copies: victims 0B (any), 0C v1 baby, 0D v1 old man with FFFF00nn ids");
    Check(!EnmCopyEligible(0x0C, 0, 0, 0xFFFF0001u, clr) && !EnmCopyEligible(0x0D, 0, 0, 0xFFFF0002u, clr) &&
              !EnmCopyEligible(0x0E, 0, 0, 0xFFFF0003u, clr) && !EnmCopyEligible(0x0F, 0, 0, 0xFFFF0004u, clr) &&
              !EnmCopyEligible(0x0B, 0, 0, clr, clr),
          "copies: never Cynthia's ghost, Jasper, Andrew, Richard; a victim without its ghost id is not copied");
    Check(EnmFollowMonster(3) && EnmFollowMonster(0x0A) && EnmFollowMonster(0x11) && !EnmFollowMonster(2) && !EnmFollowMonster(9) &&
              !EnmFollowMonster(5) && !EnmFollowMonster(4) && !EnmFollowMonster(0x10) && !EnmFollowMonster(0x0B),
          "follow: walking monsters only (no mushrooms, Tremers, Wall Men, mm, Walter; victims by the ghost path)");
    Check(EnmStanding(1) && EnmStanding(5) && !EnmStanding(9) && !EnmStanding(0x0A) && !EnmStanding(0x0B) && !EnmStanding(0x12),
          "standing: not down / HP 0 / dead / scripted");
}

static void TestBudget(void)
{
    // 4 Hummers of 60 bones (cost 150 = max(150, 2.5 x 60)); the player and Eileen take ~200 records
    EnmOrigCost h4[4] = { { 3, 60 }, { 3, 60 }, { 3, 60 }, { 3, 60 } }, h12[12];
    int made[12], i, c, used;
    Check(EnmCost(3, 60) == 150 && EnmCost(3, 0) == 150 && EnmCost(7, 100) == 250 && EnmCost(0x0C, 0) == 228,
          "cost: max(measured, 2.5 x bones)");
    Check(EnmHead(3, 60) == 90 && EnmHead(3, 0) == 90, "headroom: 1.5 x bones (3/5 of the measured cost without bones)");
    Check(EnmReserve(1408) == 540 && EnmReserve(4096) == 809, "reserve: 400 + pool / 10");
    used = 200 + 4 * 60;
    c = EnmPlanSim(h4, 4, 1, 1408 - used, 4 * 90, 1408, 6, 40, made);
    Check(c == 0, "budget: stock pool (1408), 4 Hummers, x2 -> no copy (the row says limited by memory)");
    for (i = 1; i <= 3; ++i) {
        char b[96];
        c = EnmPlanSim(h4, 4, i, 4096 - used, 4 * 90, 4096, 6, 40, made);
        sprintf_s(b, sizeof b, "budget: Auto pool (4096), 4 Hummers, x%d -> %d copies", i + 1, 4 * i);
        Check(c == 4 * i, b);
    }
    // Water Prison room 31: 12 Hummers; the pool fits only part of x4: the round robin spreads them
    for (i = 0; i < 12; ++i) h12[i].type = 3, h12[i].bones = 60;
    used = 200 + 12 * 60;
    c = EnmPlanSim(h12, 12, 3, 4096 - used, 12 * 90, 4096, 14, 40, made);
    {
        int mn = 9, mx = 0;
        for (i = 0; i < 12; ++i) mn = made[i] < mn ? made[i] : mn, mx = made[i] > mx ? made[i] : mx;
        printf("      (room 31 at x4 with the Auto pool: %d copies, %d..%d each)\n", c, mn, mx);
        Check(c > 0 && c < 36 && mx - mn <= 1, "budget: 12 Hummers at x4 -> capped, every original within one copy of the others");
    }
    c = EnmPlanSim(h12, 12, 3, 8192 - used, 12 * 90, 8192, 14, 40, made);
    Check(c == 26, "budget: 12 Hummers at x4 with the table's 8192 pool -> stopped by the list cap of 40 (14 + 26)");
    {   // round robin order: (0,1) (1,1) (2,1) (0,2) ...
        EnmStep s = { -1, 1 };
        int seq[12], m = 0;
        while (EnmPlanNext(&s, 3, 2) && m < 12) seq[m++] = s.i * 10 + s.k;
        Check(m == 6 && seq[0] == 1 && seq[1] == 11 && seq[2] == 21 && seq[3] == 2 && seq[5] == 22,
              "round robin: every original gets copy 1 before any gets copy 2");
    }
    Check(EnmPoolCount(1408, ENM_POOL_AUTO, 1) == 4096 && EnmPoolCount(1408, ENM_POOL_AUTO, 0) == 1408 &&
              EnmPoolCount(8192, ENM_POOL_AUTO, 1) == 8192 && EnmPoolCount(1408, 0, 1) == 1408 &&
              EnmPoolCount(1408, 2000, 1) == 2000 && EnmPoolCount(1408, 100, 1) == 1408 && EnmPoolCount(1408, 99999, 1) == 16384,
          "pool: Auto 4096 while a feature is on; the table's larger pool kept; 0 = stock; clamped 1408..16384");
}

static void TestRing(void)
{
    static EnmRing r;
    unsigned k;
    int ok = 1;
    EnmRingClear(&r);
    for (k = 1; k <= 300; ++k) EnmRingAdd(&r, k);
    for (k = 1; k <= 44; ++k) ok &= !EnmRingHas(&r, k);
    for (k = 45; k <= 300; ++k) ok &= EnmRingHas(&r, k);
    Check(ok, "dead ring: 256 entries, the oldest overwritten");
    EnmRingAdd(&r, 300);
    EnmRingAdd(&r, 301);
    Check(EnmRingHas(&r, 45 + 1) && EnmRingHas(&r, 301) && !EnmRingHas(&r, 45), "dead ring: a key already in is not added twice");
    for (k = 0; k < 5000; ++k) EnmRingAdd(&r, 10000 + k);
    Check(r.n >= ENM_RING_N && r.n < 2 * ENM_RING_N && EnmRingHas(&r, 14999) && !EnmRingHas(&r, 14999 - 256),
          "dead ring: no overflow over many kills");
    EnmRingClear(&r);
    Check(!EnmRingHas(&r, 14999), "dead ring: cleared (world reset / left the world)");
}

static void TestFollow(void)
{
    EnmCand c[6] = { { 900.0f * 900.0f, 0, 1 }, { 100.0f * 100.0f, 0, 1 }, { 300.0f * 300.0f, 450.0f, 1 },
                     { 500.0f * 500.0f, -100.0f, 1 }, { 50.0f * 50.0f, 0, 0 }, { 700.0f * 700.0f, 10.0f, 1 } };
    int out[8], m;
    m = EnmPickPursuers(c, 6, 800.0f, 3, out);
    Check(m == 3 && out[0] == 1 && out[1] == 3 && out[2] == 5,
          "pursuers: nearest first within 800 / 400 up or down, eligible only, at most 3");
    m = EnmPickPursuers(c, 6, 800.0f, 1, out);
    Check(m == 1 && out[0] == 1, "pursuers: FollowMax 1");
    m = EnmPickPursuers(c, 6, 1000.0f, 99, out);
    Check(m == 4, "pursuers: FollowMax capped at ENM_FOLLOW_MAX; the one 450 above never");
    Check(EnmDoorCarries(3, 3, 5, 6, 2, 0, 500, 6000), "door: a real door in the same stage carries");
    Check(!EnmDoorCarries(3, 3, 5, 6, 0, 0, 500, 6000), "door: door 0 (warp, hole, event, lift) carries nobody");
    Check(!EnmDoorCarries(5, 5, 5, 6, 2, 1, 500, 6000), "door: the Building elevator ride carries nobody");
    Check(!EnmDoorCarries(1, 1, 5, 6, 2, 0, 500, 6000) && !EnmDoorCarries(9, 9, 5, 6, 2, 0, 500, 6000),
          "door: Room 302 (stage 1) and stages outside 2..8 carry nobody");
    Check(!EnmDoorCarries(4, 3, 5, 6, 2, 0, 500, 6000), "door: a stage change carries nobody");
    Check(!EnmDoorCarries(3, 3, 6, 6, 2, 0, 500, 6000) && !EnmDoorCarries(3, 3, 5, 6, 2, 0, 7000, 6000),
          "door: a list of this room or an old list carries nobody");
    Check(fabsf(EnmFollowDue(0, 3.0f) - 3.0f) < 1e-6f && fabsf(EnmFollowDue(1, 3.0f) - 4.5f) < 1e-6f &&
              fabsf(EnmFollowDue(2, 3.0f) - 6.0f) < 1e-6f, "queue: 3 s, then +1.5 s each");
    Check(EnmArriveStep(0.0f, 100.0f) == 0 && EnmArriveStep(0.0f, 300.0f) == 1 && EnmArriveStep(9.9f, 299.0f) == 0 &&
              EnmArriveStep(10.0f, 100.0f) == 2 && EnmArriveStep(20.0f, 900.0f) == 3,
          "queue: waits while the player stands at the door (300), a ring spot after 10 s, stays home after 20 s");
    {
        const unsigned id = EnmScriptId(3, 0, 5, 4);
        Check(EnmIsWanderer(id, 3, 3, 7, 3, 0), "wanderer: a Hummer of room 5 recorded in room 7");
        Check(!EnmIsWanderer(id, 3, 3, 5, 3, 0), "wanderer: recorded at home is not one");
        Check(!EnmIsWanderer(id, 3, 3, 7, 4, 0) && !EnmIsWanderer(id, 3, 3, 7, 3, 1) && !EnmIsWanderer(id, 3, 4, 7, 3, 0),
              "wanderer: another stage / half / record stage is not one");
        Check(!EnmIsWanderer(id, 9, 3, 7, 3, 0) && !EnmIsWanderer(id, 0x0B, 3, 7, 3, 0), "wanderer: Tremers and ghosts never");
        Check(!EnmIsWanderer(0xFFFF000Au, 3, 3, 7, 3, 0) && !EnmIsWanderer(EnmCopyId(5, 4, 1), 3, 3, 7, 3, 0),
              "wanderer: ghost ids and copies never");
        Check(!EnmIsWanderer(EnmScriptId(2, 0, 14, 3), 0x0A, 2, 22, 2, 0) && EnmIsWanderer(EnmScriptId(2, 0, 14, 3), 0x0A, 2, 21, 2, 0),
              "wanderer: the train event's own record move (room 14 -> 22) is the game's, never touched");
    }
}

static void TestSpots(void)
{
    int c, ok = 1, same = 1;
    for (c = 0; c < 8; ++c) {
        float dx, dz, dx2, dz2, d;
        EnmRingSpot(0x1E001405u, c, 150.0f, 250.0f, &dx, &dz);
        EnmRingSpot(0x1E001405u, c, 150.0f, 250.0f, &dx2, &dz2);
        d = sqrtf(dx * dx + dz * dz);
        ok &= d >= 149.9f && d <= 250.1f;
        same &= dx == dx2 && dz == dz2;
    }
    Check(ok, "spots: 150..250 from the original");
    Check(same, "spots: the same on every visit (seeded by the copy id)");
}

// ---------------------------------------------------------------- v3.30 EN330
static void TestPoolAtRoom(void)
{
    Check(EnmPoolGrowAt(1408, ENM_POOL_AUTO, 1) == 4096, "room change: the stock pool grows to 4096 while a feature is on");
    Check(EnmPoolGrowAt(1408, ENM_POOL_AUTO, 0) == 0 && EnmPoolGrowAt(1408, 0, 1) == 0,
          "room change: features off or BonePool=0 -> the pool is left alone (Off = v3.29)");
    Check(EnmPoolGrowAt(4096, ENM_POOL_AUTO, 1) == 0 && EnmPoolGrowAt(8192, ENM_POOL_AUTO, 1) == 0,
          "room change: a pool already as large (Refined's world reset, the table's 8192) is never shrunk or rebuilt");
    Check(EnmPoolGrowAt(1408, 6000, 1) == 6000 && EnmPoolGrowAt(1408, 99999, 1) == 16384,
          "room change: BonePool=n grows to n (clamped 1408..16384)");
}

static void TestFollowModes(void)
{
    Check(EnmFollowModeOk(0x0A, 0x000, 0, 0) && EnmFollowModeOk(3, 0x100, 0, 5) && EnmFollowModeOk(7, 0x000, 0, 0),
          "follow modes: mode 0 always");
    Check(!EnmFollowModeOk(0x0A, 6, 0, 0x15) && EnmFollowModeOk(0x0A, 6, 1, 0x15) && EnmFollowModeOk(0x0A, 6, 1, 2),
          "follow modes: a waiting event dog (6) follows once the dogs are released (0x13D80C6 bit 0)");
    Check(!EnmFollowModeOk(0x0A, 7, 1, 0x11) && EnmFollowModeOk(0x0A, 7, 1, 3) && !EnmFollowModeOk(0x0A, 7, 0, 3),
          "follow modes: dog mode 7 needs the release and to be awake (AI not 0x11)");
    Check(!EnmFollowModeOk(0x0A, 3, 0, 0x11) && EnmFollowModeOk(0x0A, 3, 0, 4) && EnmFollowModeOk(0x0A, 0x103, 0, 4),
          "follow modes: a dog at a body (3) follows once it got up");
    Check(EnmFollowModeOk(3, 1, 0, 0) && !EnmFollowModeOk(3, 3, 0, 0) && !EnmFollowModeOk(3, 6, 1, 0),
          "follow modes: a risen Hummer (1) follows; a dead-spawned (3) or event Hummer never");
    Check(!EnmFollowModeOk(7, 6, 1, 0) && !EnmFollowModeOk(6, 6, 1, 0) && !EnmFollowModeOk(8, 7, 1, 0) &&
              !EnmFollowModeOk(5, 8, 1, 0), "follow modes: Gum Head / Wheelchair event spawns and bosses never");
}

static void TestDoorVerdict(void)
{
    Check(EnmDoorVerdict(2, 2, 5, 4, 7, 0, 100, 6000, 5) == ENM_DOOR_OK, "verdict: the user's door 7 from room 5 to 4 carries");
    Check(EnmDoorVerdict(2, 2, 5, 4, 0, 0, 5100, 6000, 5) == ENM_DOOR_WARP,
          "verdict: the user's 17:43 room change (a save load: door 0) is a warp, logged as such");
    Check(EnmDoorVerdict(2, 2, 5, 4, 7, 0, 100, 6000, -1) == ENM_DOOR_OK && EnmDoorVerdict(2, 2, 5, 4, 7, 0, 100, 6000, 9) == ENM_DOOR_FROM,
          "verdict: the room change must start in the list's room (gGame.prevRoom; -1 = not checked)");
    Check(EnmDoorVerdict(1, 1, 5, 4, 7, 0, 100, 6000, 5) == ENM_DOOR_STAGE && EnmDoorVerdict(3, 2, 5, 4, 7, 0, 100, 6000, 5) ==
              ENM_DOOR_OTHERSTAGE && EnmDoorVerdict(2, 2, 4, 4, 7, 0, 100, 6000, 4) == ENM_DOOR_SAMEROOM &&
              EnmDoorVerdict(5, 5, 5, 4, 7, 1, 100, 6000, 5) == ENM_DOOR_LIFT &&
              EnmDoorVerdict(2, 2, 5, 4, 7, 0, 6001, 6000, 5) == ENM_DOOR_STALE,
          "verdict: Room 302, a world change, the same room, a lift ride, an old list");
    {
        int st, lst, lr, r, d, e, ok = 1;
        for (st = 0; st < 10; ++st)
            for (lst = 1; lst < 4; ++lst)
                for (lr = 0; lr < 4; ++lr)
                    for (r = 1; r < 4; ++r)
                        for (d = -1; d < 3; ++d)
                            for (e = 0; e < 2; ++e)
                                ok &= (EnmDoorVerdict(st, lst, lr, r, d, e, 100, 6000, -1) == ENM_DOOR_OK) ==
                                      EnmDoorCarries(st, lst, lr, r, d, e, 100, 6000);
        Check(ok, "verdict: the same answer as v3.29's door rule when prevRoom is not known");
    }
}

static void TestKillBits(void)
{
    int w = -1;
    unsigned m = 0;
    Check(EnmKillBitBase(2) == 0x80 && EnmKillBitBase(3) == 0x100 && EnmKillBitBase(4) == 0x180 && EnmKillBitBase(5) == 0x300 &&
              EnmKillBitBase(6) == 0x20 && EnmKillBitBase(7) == 0x3C0 && EnmKillBitBase(8) == 0x2C0 && EnmKillBitBase(1) == 0,
          "kill bits: the stage bases of 0x4639C0");
    Check(EnmKillBit(2, 5, &w, &m) && w == (0x80 + 5) >> 5 && m == 1u << ((0x80 + 5) & 31),
          "kill bits: the user's room 2:4 Sniffer Dog (kill index 5) -> word 4 bit 5");
    Check(!EnmKillBit(2, 0, &w, &m) && !EnmKillBit(2, -3, &w, &m), "kill bits: index 0 / negative = no bit (as 0x463A20)");
    Check(EnmKillBit(7, 63, &w, &m) && w == 31 && !EnmKillBit(7, 64, &w, &m), "kill bits: never beyond the 32 words");
}

static void TestRevive(void)
{
    static EnmRing r;
    const unsigned clr = 0;
    int t, bad = 0, out;
    for (t = 0; t < 0x20; ++t) {
        const int want = t == 2 || t == 3 || t == 7 || t == 8 || t == 9 || t == 0x0A;
        if (EnmReviveType(t) != want) ++bad;
    }
    Check(!bad, "revive: 02 03 07 08 09 0A only (never 04 mm, 05 Wall Man, 06 Wheelchair, ghosts 0B-0F / 11, 10 Walter)");
    Check(EnmRevivePlacement(0x0A, 0, clr, clr) && !EnmRevivePlacement(0x0A, 6, clr, clr) &&
              !EnmRevivePlacement(0x0A, 0, 0xFFFF0003u, clr) && !EnmRevivePlacement(5, 0, clr, clr),
          "revive: mode 0 placements with their own script id only");
    Check(EnmReviveRoomOk(2, 0, 0) && !EnmReviveRoomOk(2, 3, 0) && !EnmReviveRoomOk(3, 0, 1) && !EnmReviveRoomOk(1, 0, 0) &&
              !EnmReviveRoomOk(9, 0, 0), "revive: never in a story room (event spawns, Walter / the Wall boss) or Room 302");
    EnmRingClear(&r);
    EnmRingAdd(&r, EnmCopyKey(2, 0, EnmCopyId(4, 1, 1)));
    EnmRingAdd(&r, EnmCopyKey(2, 0, EnmCopyId(4, 1, 2)));
    EnmRingAdd(&r, EnmCopyKey(2, 0, EnmCopyId(4, 1, 3)));
    EnmRingAdd(&r, EnmCopyKey(2, 0, EnmCopyId(5, 2, 1)));
    EnmRingAdd(&r, EnmCopyKey(2, 1, EnmCopyId(4, 1, 1)));
    EnmRingAdd(&r, EnmCopyKey(3, 0, EnmCopyId(4, 1, 1)));
    out = EnmRingRemoveRoom(&r, 2, 0, 4);
    Check(out == 3 && !EnmRingHas(&r, EnmCopyKey(2, 0, EnmCopyId(4, 1, 2))) && EnmRingHas(&r, EnmCopyKey(2, 0, EnmCopyId(5, 2, 1))) &&
              EnmRingHas(&r, EnmCopyKey(2, 1, EnmCopyId(4, 1, 1))) && EnmRingHas(&r, EnmCopyKey(3, 0, EnmCopyId(4, 1, 1))),
          "revive: the user's three killed copies of room 2:4 leave the ring; other rooms / halves / stages stay");
    {
        unsigned k;
        for (k = 0; k < 400; ++k) EnmRingAdd(&r, EnmCopyKey(2, 0, EnmCopyId(1 + (int)(k % 2) * 3, (int)(k / 6) & 0xFF, 1 + (int)(k % 3))));
        out = EnmRingRemoveRoom(&r, 2, 0, 4);
        Check(out > 0 && r.n <= ENM_RING_N && !EnmRingHas(&r, EnmCopyKey(2, 0, EnmCopyId(4, 0, 2))),
              "revive: a full (wrapped) ring compacts without losing the other rooms");
        EnmRingAdd(&r, 12345u);
        Check(EnmRingHas(&r, 12345u), "revive: the compacted ring takes new kills");
    }
}

static void TestSpotTiers(void)
{
    int i, ok = 1;
    Check(kEnmCopyTiers[0].rMin == 150.0f && kEnmCopyTiers[0].rMax == 250.0f && kEnmCopyTiers[0].minPlayer == 400.0f,
          "spot tiers: the first copy ring is v3.29's (150..250, 400 from you)");
    for (i = 1; i < ENM_COPY_TIERS; ++i) ok &= kEnmCopyTiers[i].minPlayer <= kEnmCopyTiers[i - 1].minPlayer && kEnmCopyTiers[i].minEnemy >= 60.0f;
    Check(ok, "spot tiers: later rings only relax the distance to you, never below 60 from another enemy");
    Check(kEnmCopyTiers[ENM_COPY_TIERS - 1].minPlayer >= 200.0f && kEnmDoorTiers[0][0].rMin == 60.0f && kEnmDoorTiers[1][0].rMin == 150.0f,
          "spot tiers: never nearer than 200 to you for a copy; the door rings start as v3.29's");
}

// the follower queue on the room timer at 30 / 60 / 120 / 180 ticks a second: the first one is due at 3.0 s whatever the rate
static void TestQueueRates(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    int h, ok = 1;
    for (h = 0; h < 4; ++h) {
        const float dt = 1.0f / (float)kHz[h];
        float T = 0.0f, first = -1.0f, third = -1.0f, door = 0.0f;
        int tick;
        for (tick = 0; tick < kHz[h] * 30; ++tick) {
            T += dt;
            door = T > 1.0f ? 400.0f : 0.0f;  // the player walks off the door after 1 s
            if (first < 0.0f && T >= EnmFollowDue(0, 3.0f) && EnmArriveStep(T - EnmFollowDue(0, 3.0f), door) == 1) first = T;
            if (third < 0.0f && T >= EnmFollowDue(2, 3.0f) && EnmArriveStep(T - EnmFollowDue(2, 3.0f), door) == 1) third = T;
        }
        ok &= fabsf(first - 3.0f) <= dt + 1e-4f && fabsf(third - 6.0f) <= dt + 1e-4f;
    }
    Check(ok, "queue: the arrivals come at 3.0 s and 6.0 s within one tick at 30 / 60 / 120 / 180 Hz");
}

// ---------------------------------------------------------------- v3.31 EN331
static void TestStoryGhosts(void)
{
    const unsigned clr = 0;
    int t, v, same = 1;
    for (t = 0; t < 0x20; ++t)  // story off = v3.30's rule exactly
        for (v = 0; v < 4; ++v)
            same &= EnmCopyEligible2(t, v, 0, 0xFFFF0003u, clr, 0) == EnmCopyEligible(t, v, 0, 0xFFFF0003u, clr) &&
                    EnmCopyEligible2(t, v, 0, clr, clr, 0) == EnmCopyEligible(t, v, 0, clr, clr);
    Check(same, "story ghosts: CopyStoryGhosts=0 gives v3.30's eligibility for every type / variant");
    Check(EnmCopyEligible2(0x0C, 0, 0, 0xFFFF0002u, clr, 1) && EnmCopyEligible2(0x0D, 0, 0, 0xFFFF0003u, clr, 1) &&
              EnmCopyEligible2(0x0E, 0, 0, 0xFFFF0004u, clr, 1) && EnmCopyEligible2(0x0F, 0, 0, 0xFFFF0005u, clr, 1),
          "story ghosts: Cynthia's ghost, Jasper, Andrew, Richard are copied (mode 0, their ghost id)");
    Check(!EnmCopyEligible2(0x0E, 0, 6, 0xFFFF0004u, clr, 1) && !EnmCopyEligible2(0x0C, 0, 7, 0xFFFF0002u, clr, 1) &&
              !EnmCopyEligible2(0x0F, 0, 0, clr, clr, 1) && !EnmCopyEligible2(0x10, 0, 0, clr, clr, 1) &&
              !EnmCopyEligible2(0x10, 0, 0, 0xFFFF0006u, clr, 1),
          "story ghosts: never their event modes, never without the ghost id, never Walter (10)");
    Check(EnmIsStoryGhost(0x0C, 0) && !EnmIsStoryGhost(0x0C, 1) && EnmIsStoryGhost(0x0D, 0) && !EnmIsStoryGhost(0x0D, 1) &&
              EnmIsStoryGhost(0x0E, 0) && EnmIsStoryGhost(0x0F, 0) && !EnmIsStoryGhost(0x0B, 0) && !EnmIsStoryGhost(0x11, 0),
          "story ghosts: 0C v1 (Victim 13) / 0D v1 (Victim 04) stay victims; 0B and the Patient 11 are not story ghosts");
    Check(EnmStoryCopyRoom(0x0E, 4, 40, 0) == ENM_STORY_ANDREW && EnmStoryCopyRoom(0x0E, 4, 40, 1) == ENM_STORY_OK &&
              EnmStoryCopyRoom(0x0E, 8, 8, 0) == ENM_STORY_OK && EnmStoryCopyRoom(0x0F, 5, 3, 1) == ENM_STORY_RICHARD &&
              EnmStoryCopyRoom(0x0F, 5, 8, 0) == ENM_STORY_OK && EnmStoryCopyRoom(0x0C, 4, 3, 0) == ENM_STORY_OK,
          "story rooms: Andrew not in stage 4 before flag 2DC (0x4CE050), Richard not in room 5:3 (flag 3BF)");
    Check(EnmGhostType(0x0B) && EnmGhostType(0x0F) && !EnmGhostType(0x0A) && !EnmGhostType(0x10) && !EnmGhostType(0x11),
          "ghost types 0B..0F as 0x463910 (the Patient dies)");
}
static void TestBeside(void)
{
    Check(EnmCopyBeside(0x0B, 0x0A) && EnmCopyBeside(0x0B, 9) && EnmCopyBeside(0x0B, 0x0B) && EnmCopyBeside(0x0E, 8) &&
              EnmCopyBeside(0x0B, 1) && EnmCopyBeside(0x0B, 0x0F) && !EnmCopyBeside(0x0B, 0x10) && !EnmCopyBeside(0x0B, 0x12),
          "beside: a downed (9 / 0A) or pinned (0B) ghost still gets its copies (the user's rooms 5:14 and 5:6)");
    Check(!EnmCopyBeside(0x03, 9) && !EnmCopyBeside(0x0A, 0x0A) && !EnmCopyBeside(0x11, 0x0B) && EnmCopyBeside(0x03, 1),
          "beside: monsters and the Patient as v3.30 (standing only)");
    Check(!EnmCopyKilled(0x0B, 0x0B) && !EnmCopyKilled(0x0D, 0x0B) && EnmCopyKilled(0x0A, 0x0B) && EnmCopyKilled(0x11, 0x0B) &&
              !EnmCopyKilled(0x0A, 0x0A),
          "dead ring: a pinned ghost copy is never 'killed'; monsters and the Patient are");
}
// 0x4853F0 frame by frame (the stock record update) against the closed form
static void StockRegen(float *cd, float *hp, float mx, int diff, float dt)
{
    float v = *cd;
    if (v > 0.0f) {
        v -= dt;
        if (v < 0.0f) v = 0.0f;
        *cd = v;
        return;
    }
    if (*hp < mx) {
        v = (float)(1 << diff) * dt + *hp;
        *hp = v < mx ? v : mx;
    }
}
static void TestGhostDecay(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    static const float kCase[4][4] = { { 90.0f, 0.0f, 300.0f, 100.0f }, { 5.0f, 120.0f, 400.0f, 3.0f },
                                       { 0.0f, 50.0f, 200.0f, 40.0f }, { 110.0f, 0.0f, 800.0f, 400.0f } };
    int h, c, d, ok = 1;
    for (d = 1; d <= 3; ++d)
        for (c = 0; c < 4; ++c)
            for (h = 0; h < 4; ++h) {
                const float dt = 1.0f / (float)kHz[h];
                float cd = kCase[c][0], hp = kCase[c][1], cd2 = cd, hp2 = hp;
                const int n = (int)(kCase[c][3] * kHz[h] + 0.5f);
                int i;
                for (i = 0; i < n; ++i) StockRegen(&cd, &hp, kCase[c][2], d, dt);
                EnmGhostDecay(&cd2, &hp2, kCase[c][2], d, kCase[c][3]);
                // one frame of slack (the stock frame that reaches 0 does not heal) plus the float sum of up to 72,000
                // frames (0.03 %)
                ok &= fabsf(cd - cd2) <= dt + 1e-3f && fabsf(hp - hp2) <= (float)(1 << d) * 2.0f * dt + 0.0003f * hp2 + 0.05f;
            }
    Check(ok, "decay: the closed form = 0x4853F0 frame by frame at 30 / 60 / 120 / 180 Hz, Easy .. Hard (within a frame)");
    {
        float cd = 90.0f, hp = 0.0f;
        EnmGhostDecay(&cd, &hp, 300.0f, 2, 30.0f);
        Check(cd == 60.0f && hp == 0.0f, "decay: 30 s away from a 90 s down: 60 s left, no heal yet");
        EnmGhostDecay(&cd, &hp, 300.0f, 2, 70.0f);
        Check(cd == 0.0f && hp == 40.0f, "decay: 10 s past the countdown at Normal (4 / s): HP 40");
        EnmGhostDecay(&cd, &hp, 300.0f, 2, 1000.0f);
        Check(hp == 300.0f, "decay: capped at the max");
        EnmGhostDecay(&cd, &hp, 300.0f, 2, -5.0f);
        Check(hp == 300.0f && cd == 0.0f, "decay: a negative time changes nothing");
    }
}
static void TestClock(void)
{
    static const int kHz[4] = { 30, 60, 120, 180 };
    int h, ok = 1;
    for (h = 0; h < 4; ++h) {
        const float dt = 1.0f / (float)kHz[h];
        float timer = 0.0f, prev = 0.0f, clock = 0.0f;
        int i;
        for (i = 0; i < kHz[h] * 20; ++i) {  // 20 s in a room, a room change (the timer restarts at 2.0), 10 s more
            if (i == kHz[h] * 10) timer = 2.0f;
            else timer += dt;
            clock += EnmClockStep(prev, timer);
            prev = timer;
        }
        ok &= fabsf(clock - (20.0f - dt)) <= 0.01f;
    }
    Check(ok, "clock: the room timer's steps add up to the time at 30 / 60 / 120 / 180 Hz; a room change step counts 0");
    Check(EnmClockStep(5.0f, 4.0f) == 0.0f && EnmClockStep(1.0f, 30.0f) == 1.0f && EnmClockStep(1.0f, 1.0f) == 0.0f,
          "clock: a restart counts 0, a stall at most 1 s");
}
static void TestSnapStore(void)
{
    static EnmSnapStore st;
    unsigned char rec[ENM_REC_SIZE];
    int i, ok = 1;
    EnmSnapClear(&st);
    for (i = 0; i < ENM_REC_SIZE; ++i) rec[i] = (unsigned char)i;
    EnmSnapPut(&st, 0x1234u, 14, 5.0f, rec);
    rec[0x26] = 0x0B;
    EnmSnapPut(&st, 0x1234u, 14, 9.0f, rec);
    Check(EnmSnapCount(&st) == 1 && EnmSnapFind(&st, 0x1234u)->clock == 9.0f && EnmSnapFind(&st, 0x1234u)->rec[0x26] == 0x0B,
          "store: the same copy replaces its record (the latest exit)");
    for (i = 0; i < ENM_SNAP_N + 10; ++i) EnmSnapPut(&st, 0x10000u + (unsigned)i, 5, (float)i, rec);
    Check(EnmSnapCount(&st) == ENM_SNAP_N, "store: full at ENM_SNAP_N (1024)");
    Check(ENM_SNAP_N >= 40 * 3 * 8, "store: room for 40 ghost originals x3 copies in 8 rooms (EN331-002)");
    for (i = 0; i < 11; ++i) ok &= EnmSnapFind(&st, i ? 0x10000u + (unsigned)(i - 1) : 0x1234u) == 0;
    ok &= EnmSnapFind(&st, 0x10000u + ENM_SNAP_N + 9) != 0 && EnmSnapFind(&st, 0x10000u + 11) != 0;
    Check(ok, "store: the oldest go first");
    EnmSnapClear(&st);
    Check(EnmSnapCount(&st) == 0 && !EnmSnapFind(&st, 0x10005u), "store: cleared (a world reset, a new world, Copies Off)");
    {
        const unsigned k1 = EnmCopyKey(5, 1, EnmCopyId(14, 3, 1)), k2 = EnmCopyKey(5, 1, EnmCopyId(14, 3, 2)),
                       k3 = EnmCopyKey(5, 0, EnmCopyId(14, 3, 1));
        Check(k1 != k2 && k1 != k3, "store: keyed per copy, stage and half (room 5:14's victim copies 1 and 2)");
    }
}
static void TestFlyer(void)
{
    // the user's room 5:4: Hummers at y -720 over a floor at about 0 (y grows downwards)
    Check(EnmIsFlyer(0x03, -720.0f, 0.0f) && !EnmIsFlyer(0x03, -100.0f, 0.0f) && !EnmIsFlyer(0x0A, -720.0f, 0.0f),
          "flyer: a Hummer 720 above its floor; not one near the floor, not another type");
    Check(EnmFlyerFloorOk(0.0f, 0.0f) && EnmFlyerFloorOk(0.0f, -350.0f) && EnmFlyerFloorOk(0.0f, 380.0f) &&
              !EnmFlyerFloorOk(0.0f, -500.0f) && !EnmFlyerFloorOk(0.0f, 450.0f),
          "flyer: the floor under the copy within 400 of the original's floor");
    Check(EnmFlyerY(-720.0f, 0.0f, 0.0f) == -720.0f && EnmFlyerY(-720.0f, 0.0f, -200.0f) == -920.0f,
          "flyer: the copy flies as high above its floor as the original");
}

// v3.33 EN333: the event dogs are copied in their spawn mode; the rest as v3.32
static void TestEventDogs(void)
{
    const unsigned clr = 0xFFFFFFFFu;
    // Subway World 2:2: dog 1 mode word 7, dogs 2 / 3 mode word 6 (enemy_placements.tsv); 2:4 dog mode 0; 2nd visit 0x100
    Check(EnmEventDog(0x0A, 7) && EnmEventDog(0x0A, 6) && EnmEventDog(0x0A, 3) && EnmEventDog(0x0A, 0x107) &&
              !EnmEventDog(0x0A, 0) && !EnmEventDog(0x0A, 0x100) && !EnmEventDog(0x0A, 1) && !EnmEventDog(0x0A, 8) &&
              !EnmEventDog(0x07, 6) && !EnmEventDog(0x03, 3),
          "event dogs: Sniffer modes 3 / 6 / 7 only (no other type, no other mode)");
    Check(EnmCopyEligible3(0x0A, 0, 7, clr, clr, 1, 1) && EnmCopyEligible3(0x0A, 0, 6, clr, clr, 1, 1) &&
              EnmCopyEligible3(0x0A, 0, 3, clr, clr, 0, 1),
          "event dogs: the 2:2 dogs are eligible (their own script id)");
    Check(!EnmCopyEligible3(0x0A, 0, 7, clr, clr, 1, 0) && !EnmCopyEligible3(0x0A, 0, 6, 0xFFFF0003u, clr, 1, 1),
          "event dogs: not with CopyEventDogs=0, not with an id override");
    Check(EnmCopyEligible3(0x0A, 0, 0, clr, clr, 1, 1) && EnmCopyEligible3(0x0A, 1, 0x100, clr, clr, 1, 0) &&
              !EnmCopyEligible3(0x07, 0, 6, clr, clr, 1, 1) && !EnmCopyEligible3(0x10, 0, 6, clr, clr, 1, 1) &&
              !EnmCopyEligible3(0x05, 0, 8, clr, clr, 1, 1) && !EnmCopyEligible3(0x03, 0, 1, clr, clr, 1, 1),
          "event dogs: ordinary dogs as before; the Gum Head's / Walter's event mode 6, the Wall boss and a risen Hummer stay out");
    Check(EnmCopyModeWord(0x0A, 7, 1) == 7 && EnmCopyModeWord(0x0A, 6, 1) == 6 && EnmCopyModeWord(0x0A, 0x103, 1) == 0x103,
          "event dogs: the copy keeps the whole mode word (sleeps and wakes with flag 130)");
    Check(EnmCopyModeWord(0x0A, 7, 0) == 0 && EnmCopyModeWord(0x0A, 0x100, 1) == 0x100 && EnmCopyModeWord(0x0B, 0x306, 1) == 0x300 &&
              EnmCopyModeWord(0x07, 0x106, 1) == 0x100,
          "event dogs: every other copy is spawn mode 0 with its variant (v3.29)");
    Check(EnmCopyModeOk(0x0A, 7, 1) && EnmCopyModeOk(0x0A, 0, 1) && EnmCopyModeOk(0x0B, 0x300, 1) && !EnmCopyModeOk(0x0A, 7, 0) &&
              !EnmCopyModeOk(0x07, 6, 1),
          "event dogs: a standing original in its event mode gets copies (the dogs only)");
}
// v3.33 EN333: Follow: the per-type vertical limit, the kept list, the files, the arena rule
static void TestFollow333(void)
{
    EnmCand2 c[4];
    int out[6], m;
    // a flying Hummer 720 above her (Building 5:17), a dog 150 below, a dog 500 above
    c[0].d2 = 300.0f * 300.0f, c[0].dy = -720.0f, c[0].ok = 1, c[0].dyMax = EnmFollowDyMax(0x03);
    c[1].d2 = 500.0f * 500.0f, c[1].dy = 150.0f, c[1].ok = 1, c[1].dyMax = EnmFollowDyMax(0x0A);
    c[2].d2 = 100.0f * 100.0f, c[2].dy = -500.0f, c[2].ok = 1, c[2].dyMax = EnmFollowDyMax(0x0A);
    c[3].d2 = 200.0f * 200.0f, c[3].dy = -1200.0f, c[3].ok = 1, c[3].dyMax = EnmFollowDyMax(0x03);
    m = EnmPickPursuers2(c, 4, 800.0f, 3, out);
    Check(m == 2 && out[0] == 0 && out[1] == 1, "follow: a flying Hummer 720 above is noted (1100), a dog 500 above is not (400), nor a Hummer 1200 above");
    Check(EnmCandDyOk(399.0f, 0.0f) && !EnmCandDyOk(401.0f, 0.0f) && EnmCandDyOk(-399.0f, -1.0f),
          "follow: a limit of 0 or less is v3.32's 400");
    {   // EnmPickPursuers2 = EnmPickPursuers for the v3.32 limit
        EnmCand a[3] = { { 90000.0f, 10.0f, 1 }, { 10000.0f, 450.0f, 1 }, { 40000.0f, -20.0f, 1 } };
        EnmCand2 b[3];
        int o1[6], o2[6], m1, m2, i, same = 1;
        for (i = 0; i < 3; ++i) b[i].d2 = a[i].d2, b[i].dy = a[i].dy, b[i].ok = a[i].ok, b[i].dyMax = 0.0f;
        m1 = EnmPickPursuers(a, 3, 800.0f, 3, o1);
        m2 = EnmPickPursuers2(b, 3, 800.0f, 3, o2);
        for (i = 0; i < m1 && i < m2; ++i) same &= o1[i] == o2[i];
        Check(m1 == m2 && same && m1 == 2, "follow: the per-type picker matches v3.32's for the 400 limit");
    }
    Check(EnmPurKeep(1, 1, 2) && !EnmPurKeep(1, 1, 0) && !EnmPurKeep(1, 0, 2) && !EnmPurKeep(0, 1, 2),
          "follow: an empty list is kept only during the teardown of the room it holds someone of (an empty room stays fresh)");
    {   // the door verdict of an empty room left 49 s after entering: v3.32 "too old", v3.33 "nobody near" (fresh, empty)
        unsigned age = 0;
        int ticks, kept = 0, n = 0;
        for (ticks = 0; ticks < 49 * 60; ++ticks) {
            if (EnmPurKeep(1, ticks > 0, n)) ++kept;
            else age = 0;  // refreshed
            age += 17;
        }
        Check(!kept && EnmDoorVerdict(2, 2, 3, 2, 3, 0, age, 6000, 3) == ENM_DOOR_OK,
              "follow: leaving an empty room after 49 s: the list is fresh (the door decides 'nobody near'), not 'too old'");
    }
    Check(EnmWholeKey(0xF003F068u) == 0xF000F068u && EnmWholeKey(0xF000F068u) == 0xF000F068u &&
              EnmWholeKey(0xF8020123u | 0xF000u) == (0xF800F123u) && EnmWholeKey(0x00010068u) == 0 && EnmWholeKey(0) == 0,
          "files: the whole key of a container entry (0xF0NNFkkk -> 0xF000Fkkk, the second table's bit kept)");
    {
        unsigned k[4];
        int n = 0;
        n = EnmKeyAdd(k, n, 4, 0xF000F068u);  // the hyena model
        n = EnmKeyAdd(k, n, 4, 0xF001F068u);  // its second file: the same container
        n = EnmKeyAdd(k, n, 4, 0xF006F068u);  // a motion: the same container
        n = EnmKeyAdd(k, n, 4, 0);            // none
        n = EnmKeyAdd(k, n, 4, 0xF003F11Fu);  // the Gum Head's other motion container
        n = EnmKeyAdd(k, n, 4, 0xF000F071u);
        n = EnmKeyAdd(k, n, 4, 0xF000F0BBu);
        n = EnmKeyAdd(k, n, 4, 0xF000F121u);  // beyond max: dropped
        Check(n == 4 && k[0] == 0xF000F068u && k[1] == 0xF000F11Fu && k[2] == 0xF000F071u && k[3] == 0xF000F0BBu,
              "files: whole keys collected once each, 0 skipped, at most max");
    }
    Check(EnmFileBlock(1) == 0x800 && EnmFileBlock(0x800) == 0x800 && EnmFileBlock(0x801) == 0x1000 && EnmFileBlock(0) == 0 &&
              EnmFileBlock(1782000) == 1783808u, "files: a file takes its size rounded up to 0x800 (FileRequestAsync)");
    // the user's Subway: 64 MB arena, 24.6 MB used (spawn notes_v2) = 39.4 MB free; the hyena 1.7 MB
    Check(EnmArenaFits(39u << 20, 1783808u, ENM_ARENA_KEEP) && !EnmArenaFits(31u << 20, 1783808u, ENM_ARENA_KEEP) &&
              !EnmArenaFits(1000u, 2000u, 0) && EnmArenaFits(2000u, 2000u, 0),
          "files: loaded only when 30 MB stay free after them (Subway 39 MB free: yes; 31 MB: no); never below zero");
}

// ---------------------------------------------------------------- v3.34 EN334
// Revive Enemies: the released event dogs (Subway 2:2), their copies awake
static void TestRevive334(void)
{
    const unsigned clr = 0xFFFFFFFFu;
    // the 2:2 script: 02020001 mode 7, 02020002 / 02020003 mode 6 (all three counted as event spawns by EnmRevCb)
    static const unsigned kMw[3] = { 7, 6, 6 };
    int i, ev = 0, rel = 0, relOff = 0;
    for (i = 0; i < 3; ++i) {
        ev += (kMw[i] & 0xF) == 6 || (kMw[i] & 0xF) == 7;
        rel += EnmReviveEventDog(0x0A, kMw[i], clr, clr, 1);
        relOff += EnmReviveEventDog(0x0A, kMw[i], clr, clr, 0);
    }
    Check(ev == 3 && rel == 3 && relOff == 0, "revive 334: the three 2:2 dogs are released event dogs once flag 130 is set, none before");
    Check(EnmReviveRoomOk2(2, ev, rel, 0) && !EnmReviveRoomOk2(2, ev, relOff, 0),
          "revive 334: room 2:2 revives once flag 130 is set (v3.33: 'a story room (3 event spawn(s))'); before, it stays a story room");
    Check(!EnmReviveRoomOk2(2, 2, 1, 0) && !EnmReviveRoomOk2(2, 1, 1, 1) && !EnmReviveRoomOk2(9, 3, 3, 0) && EnmReviveRoomOk2(5, 0, 0, 0),
          "revive 334: another event spawn (a Gum Head's mode 6) or a boss still makes a story room; stages 2..8 only");
    Check(EnmReviveRoomOk2(3, 0, 2, 0) == EnmReviveRoomOk(3, 0, 0), "revive 334: more released dogs than counted events never go below zero");
    Check(!EnmReviveEventDog(0x0A, 7, 0xFFFF0003u, clr, 1) && !EnmReviveEventDog(0x0A, 0, clr, clr, 1) &&
              !EnmReviveEventDog(0x07, 6, clr, clr, 1) && !EnmReviveEventDog(0x10, 6, clr, clr, 1) &&
              EnmReviveEventDog(0x0A, 3, clr, clr, 1) && EnmReviveEventDog(0x0A, 0x107, clr, clr, 1),
          "revive 334: only a Sniffer in mode 3 / 6 / 7 with its own id (an ordinary dog revives by the v3.30 rule; no other type)");
    Check(EnmRevivePlacement(0x0A, 0, clr, clr) && !EnmRevivePlacement(0x0A, 7, clr, clr),
          "revive 334: the v3.30 placement rule is unchanged (mode 0 only)");
    Check(EnmCopyModeWord2(0x0A, 7, 1, 1) == 0 && EnmCopyModeWord2(0x0A, 6, 1, 1) == 0 && EnmCopyModeWord2(0x0A, 0x107, 1, 1) == 0x100 &&
              EnmCopyModeWord2(0x0A, 7, 1, 0) == 7 && EnmCopyModeWord2(0x0A, 6, 0, 0) == 0 && EnmCopyModeWord2(0x07, 0x106, 1, 1) == 0x100,
          "revive 334: a released event dog's copy is made awake (mode 0, its variant); before the flag it keeps v3.33's word");
}
// Follow: copies pursue as monsters do; FollowMax; a pack and the bone budget
static void TestCopyPursuers(void)
{
    const unsigned cid = EnmCopyId(5, 2, 3);
    Check(EnmCopyRoomOf(cid) == 5 && EnmCopyNOf(cid) == 2 && EnmCopyKOf(cid) == 3 && EnmCopyRoomOf(EnmCopyId(42, 30, 1)) == 42,
          "copy follow: a copy id holds its original's room and object number (its descriptor anywhere in the stage)");
    // copy, ghost, victims, type, mode word, released, AI state, script id, kill index
    Check(EnmPursuerWhy(1, 0, 0, 0x0A, 0, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy(1, 0, 0, 0x03, 1, 0, 0, 0, 0) == ENM_PUR_YES &&
              EnmPursuerWhy(1, 0, 0, 0x07, 0, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy(1, 0, 0, 0x11, 0, 0, 0, 0, 0) == ENM_PUR_YES,
          "copy follow: a copy of a dog, a risen Hummer, a Gum Head, a Patient follows (no script id, no kill index needed)");
    Check(EnmPursuerWhy(1, 0, 1, 0x02, 0, 0, 0, 0, 0) == ENM_PUR_OTHER && EnmPursuerWhy(1, 0, 1, 0x0B, 0, 0, 0, 0, 0) == ENM_PUR_OTHER &&
              EnmPursuerWhy(1, 0, 1, 0x0D, 0x100, 0, 0, 0, 0) == ENM_PUR_OTHER,
          "copy follow: copies of mushrooms and of victims never (not a follow monster)");
    Check(EnmPursuerWhy(1, 0, 0, 0x0A, 6, 0, 0, 0, 0) == ENM_PUR_EVENT && EnmPursuerWhy(1, 0, 0, 0x0A, 7, 1, 0x11, 0, 0) == ENM_PUR_EVENT &&
              EnmPursuerWhy(1, 0, 0, 0x0A, 6, 1, 0x15, 0, 0) == ENM_PUR_YES,
          "copy follow: an event dog's copy only once released and not lying as a body (AI 0x11), as its original");
    Check(EnmPursuerWhy(0, 0, 0, 0x0A, 0, 0, 0, 1, 6) == ENM_PUR_YES && EnmPursuerWhy(0, 0, 0, 0x0A, 0, 0, 0, 1, 0) == ENM_PUR_NOKILL &&
              EnmPursuerWhy(0, 0, 0, 0x0A, 0, 0, 0, 0, 6) == ENM_PUR_OTHER && EnmPursuerWhy(0, 0, 0, 0x02, 0, 0, 0, 1, 6) == ENM_PUR_OTHER,
          "copy follow: monsters as v3.33 (a script id and a kill index; no mushrooms)");
    Check(EnmPursuerWhy(0, 1, 0, 0x0B, 0, 0, 0, 0, 0) == ENM_PUR_OTHER && EnmPursuerWhy(0, 1, 1, 0x0B, 0, 0, 0, 0, 0) == ENM_PUR_YES &&
              EnmPursuerWhy(0, 1, 1, 0x0B, 3, 0, 0, 0, 0) == ENM_PUR_EVENT,
          "copy follow: victims as v3.33 (Monsters + Victims, spawn mode 0)");
    Check(ENM_FOLLOW_DEFAULT == 32 && ENM_FOLLOW_MAX == 32 && EnmFollowMaxIni(3) == 3 && EnmFollowMaxIni(0) == 1 &&
              EnmFollowMaxIni(9) == 9 && EnmFollowMaxIni(40) == 32 && EnmFollowMaxIni(-5) == 1 && EnmFollowMaxIni(6) == 6,
          "copy follow: FollowMax 32 by default (v3.38 EN338; was 6), the ini 1..32 (an old FollowMax=3 / 6 / 8 reads as before)");
    {   // room 5: its 2 dogs and their 6 copies (x4) near her, the original 02050003 dead: the nearest 6 follow, any copy alone
        EnmCand2 c[8];
        int out[8], m, i, ok = 1;
        for (i = 0; i < 8; ++i) c[i].d2 = (100.0f + 60.0f * i) * (100.0f + 60.0f * i), c[i].dy = 0.0f, c[i].ok = 1, c[i].dyMax = 0.0f;
        c[0].ok = 0;  // the original: dead (its copies follow on their own)
        m = EnmPickPursuers2(c, 8, 800.0f, 6, out);  // (v3.34's FollowMax 6)
        for (i = 0; i < m; ++i) ok &= out[i] == i + 1;
        Check(m == 6 && ok, "copy follow: with the original dead its 6 copies are the 6 nearest pursuers (FollowMax 6)");
        m = EnmPickPursuers2(c, 8, 800.0f, 99, out);
        Check(m == 7, "copy follow: FollowMax 99 is capped at ENM_FOLLOW_MAX (all 7 standing here)");
    }
    {   // the bone budget of a pack: Subway 2:2 with its 3 dogs and 9 copies (38 bones each), 8 dogs following in
        int k, freeRec = 3899 - 9 * 38, head = 12 * EnmHead(0x0A, 38), fit = 0, fitStock = 0, freeS = 1408 - 200 - 12 * 38;
        int headS = head;
        for (k = 0; k < 8; ++k) {
            if (EnmFits(freeRec, head, EnmCost(0x0A, 0), 4096)) ++fit, freeRec -= 38, head += EnmHead(0x0A, 38);
            if (EnmFits(freeS, headS, EnmCost(0x0A, 0), 1408)) ++fitStock, freeS -= 38, headS += EnmHead(0x0A, 38);
        }
        Check(fit == 8 && fitStock == 0,
              "copy follow: 8 dogs fit the Auto pool (4096) of a full room 2:2; the stock pool (1408) makes them wait (no harm)");
    }
    {   // 8 followers come in at delay + 1.5 s each, within one tick at 30 / 60 / 120 / 180 Hz
        static const int kHz[4] = { 30, 60, 120, 180 };
        int h, ok = 1;
        for (h = 0; h < 4; ++h) {
            const float dt = 1.0f / (float)kHz[h];
            float T = 0.0f, eighth = -1.0f;
            int tick;
            for (tick = 0; tick < kHz[h] * 30; ++tick) {
                T += dt;
                if (eighth < 0.0f && T >= EnmFollowDue(7, 3.0f) && EnmArriveStep(T - EnmFollowDue(7, 3.0f), 400.0f) == 1) eighth = T;
            }
            ok &= fabsf(eighth - 13.5f) <= dt + 1e-4f;
        }
        Check(ok, "copy follow: the 8th follower comes in at 13.5 s within one tick at 30 / 60 / 120 / 180 Hz");
    }
}
// the copies' ledger: a copy that followed lives in its new room; resets
static void SetRec(unsigned char *rec, unsigned id, int type, int state, int stage, int room, float hp)
{
    int i;
    for (i = 0; i < ENM_REC_SIZE; ++i) rec[i] = 0;
    memcpy(rec + 0x20, &id, 4);
    rec[0x24] = (unsigned char)type, rec[0x25] = 0;
    rec[0x26] = (unsigned char)state, rec[0x27] = 0;
    memcpy(rec + 0x28, &hp, 4);
    rec[0x36] = (unsigned char)stage, rec[0x37] = (unsigned char)room;
}
static void TestCopyLedger(void)
{
    static EnmCwLedger L;
    unsigned char rec[ENM_REC_SIZE];
    const unsigned c1 = EnmCopyId(5, 2, 1), c2 = EnmCopyId(5, 2, 2);
    const unsigned k1 = EnmCopyKey(2, 0, c1), k2 = EnmCopyKey(2, 0, c2);
    EnmCw *e;
    int i, ev = 0;
    EnmCwClear(&L);
    Check(EnmCwCount(&L) == 0 && !EnmCwFind(&L, k1), "ledger: empty after a clear (world reset, load, New Game, leaving the world)");
    // c1 follows the player from room 5 into room 25 (the arrival writes its record there)
    SetRec(rec, c1, 0x0A, 1, 2, 25, 120.0f);
    Check(EnmCwPut(&L, k1, c1, 25, 0x0A, 0, rec) == 0 && (e = EnmCwFind(&L, k1)) != 0 && e->room == 25 && e->cid == c1 && e->type == 0x0A,
          "ledger: a copy that came in lives in room 25");
    Check(!EnmCwFind(&L, k2) && !EnmCwFind(&L, EnmCopyKey(2, 1, c1)) && !EnmCwFind(&L, EnmCopyKey(3, 0, c1)),
          "ledger: only that copy (k), that stage, that half: its siblings are still made in room 5");
    // the player leaves 25 for 2: the copy's destroy writes its record (room 25, down, HP 83): kept there
    SetRec(rec, c1, 0x0A, 9, 2, 25, 83.0f);
    Check(EnmCwOnRecord(&L, k1, rec) == 0 && (e = EnmCwFind(&L, k1)) != 0 && e->room == 25 && e->rec[0x26] == 9,
          "ledger: leaving its room keeps it there with its record (state, HP): made again on return, never in room 5");
    // it follows again into room 24 and arrives there
    SetRec(rec, c1, 0x0A, 1, 2, 24, 83.0f);
    EnmCwPut(&L, k1, c1, 24, 0x0A, 0, rec);
    Check(EnmCwCount(&L) == 1 && EnmCwFind(&L, k1)->room == 24, "ledger: following again moves it (one entry per copy)");
    // a copy not in the ledger: its record is none of the ledger's business
    SetRec(rec, c2, 0x0A, 1, 2, 5, 170.0f);
    Check(EnmCwOnRecord(&L, k2, rec) == -1 && EnmCwCount(&L) == 1, "ledger: a copy that never followed stays out of it (made in its room)");
    // it dies in room 24: its exit record says 0x0B: it leaves the ledger (the dead ring keeps it dead)
    SetRec(rec, c1, 0x0A, 0x0B, 2, 24, 0.0f);
    Check(EnmCwOnRecord(&L, k1, rec) == 1 && !EnmCwFind(&L, k1) && EnmCwCount(&L) == 0, "ledger: a copy that died leaves it");
    // full: the oldest goes
    for (i = 0; i <= ENM_CW_N; ++i) {
        const unsigned c = EnmCopyId(1 + i % 60, i / 60, 1 + i % 3);
        SetRec(rec, c, 0x07, 1, 4, 7, 50.0f);
        ev += EnmCwPut(&L, EnmCopyKey(4, 0, c), c, 7, 0x07, 0, rec);
    }
    Check(ev == 1 && EnmCwCount(&L) == ENM_CW_N && !EnmCwFind(&L, EnmCopyKey(4, 0, EnmCopyId(1, 0, 1))),
          "ledger: ENM_CW_N (128) entries; one more evicts the copy that followed longest ago (it is made in its room again)");
    EnmCwClear(&L);
    Check(EnmCwCount(&L) == 0 && L.age == 0, "ledger: the resets empty it (copies are never saved)");
}
// wanderers: where one is put, drawn classes
static void TestWanderSpot(void)
{
    Check(EnmSnapKind(-150.0f, -150.0f) == ENM_WSPOT_RECORD && EnmSnapKind(-150.0f, -146.0f) == ENM_WSPOT_RECORD &&
              EnmSnapKind(-150.0f, -230.0f) == ENM_WSPOT_FLOOR && EnmSnapKind(-150.0f, -100.0f) == ENM_WSPOT_FLOOR,
          "wanderer: a floor within 5 of its record keeps the spot; else it is put onto the floor (one under it is lifted)");
    Check(kEnmWspot[ENM_WSPOT_RING] != 0 && kEnmWspot[ENM_WSPOT_FLYER] != 0, "wanderer: every spot kind has a log name");
    Check(EnmClassDrawn(1) && EnmClassDrawn(9) && EnmClassDrawn(0x0B) && EnmClassDrawn(0x0C) && EnmClassDrawn(0x12) &&
              !EnmClassDrawn(0) && !EnmClassDrawn(0x0D) && !EnmClassDrawn(0x10) && !EnmClassDrawn(-1),
          "wanderer: the classes the game draws (EnemyIsActive 0x45CA40: 1..0x0C, 0x12)");
}

// v3.35 GH335: victims pursue, a victim that followed before follows again, victims' copies follow, late copies, the
// down countdown is never shortened by a hit (30 / 60 / 120 / 180 Hz)
static void TestGhosts335(void)
{
    // -- the root of "one room only": the real ghost made from a follower placeholder has mode word variant << 8 | 4
    Check(EnmPursuerWhy(0, 1, 1, 0x0B, 0x304, 0, 0, 0, 0) == ENM_PUR_EVENT &&
              EnmPursuerWhy2(0, 1, 1, 0x0B, 3, 0x304, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy2(0, 1, 1, 0x0B, 3, 0x300, 0, 0, 0, 0) == ENM_PUR_YES,
          "pursue: a victim that followed once (mode word 304, EnemyGhostRealFromPlaceholder) was an 'event spawn' in v3.34; now it follows again");
    Check(EnmPursuerWhy2(0, 1, 1, 0x0B, 0, 0x003, 0, 0, 0, 0) == ENM_PUR_EVENT && EnmPursuerWhy2(0, 1, 0, 0x0B, 3, 0x304, 0, 0, 0, 0) == ENM_PUR_OTHER,
          "pursue: a victim's event mode (3) still never; Monsters only: no victims");
    Check(EnmPursuerWhy2(1, 0, 1, 0x0B, 2, 0x200, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy2(1, 0, 1, 0x0D, 1, 0x100, 0, 0, 0, 0) == ENM_PUR_YES &&
              EnmPursuerWhy2(1, 0, 0, 0x0B, 2, 0x200, 0, 0, 0, 0) == ENM_PUR_OTHER,
          "pursue: a victim's copy follows as a copy (Monsters + Victims); not with Monsters only");
    Check(EnmPursuerWhy2(1, 0, 1, 0x0E, 0, 0, 0, 0, 0, 0) == ENM_PUR_OTHER && EnmPursuerWhy2(1, 0, 1, 0x0C, 0, 0, 0, 0, 0, 0) == ENM_PUR_OTHER &&
              EnmPursuerWhy2(1, 0, 1, 0x0A, 0, 0, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy2(0, 0, 1, 0x0A, 0, 0, 0, 0, 1, 6) == ENM_PUR_YES,
          "pursue: the story ghosts' copies (Andrew, Cynthia v0) never; monsters and their copies as v3.34");
    // -- picking: victims 1500 away, pursuers first and anywhere in the room
    {
        EnmCand3 c[5];
        int out[8], m, i;
        for (i = 0; i < 5; ++i) c[i].dy = 0.0f, c[i].dyMax = 0.0f, c[i].ok = 1, c[i].prio = 0, c[i].r2 = 800.0f * 800.0f;
        c[0].d2 = 300.0f * 300.0f;                                       // a dog near you
        c[1].d2 = 1200.0f * 1200.0f, c[1].r2 = 1500.0f * 1500.0f;        // a victim 1200 away (v3.34: too far)
        c[2].d2 = 2500.0f * 2500.0f, c[2].r2 = 1500.0f * 1500.0f, c[2].prio = 1, c[2].dy = 900.0f;  // a pursuer far away, above
        c[3].d2 = 900.0f * 900.0f;                                       // a dog too far
        c[4].d2 = 100.0f * 100.0f, c[4].ok = 0;                          // down
        m = EnmPickPursuers3(c, 5, 6, out);
        Check(m == 3 && out[0] == 2 && out[1] == 0 && out[2] == 1,
              "pick: the pursuer first (2500 away, 900 above), then the dog near you, then the victim 1200 away (FollowRangeVictims 1500)");
        m = EnmPickPursuers3(c, 5, 1, out);
        Check(m == 1 && out[0] == 2, "pick: FollowMax 1: the pursuer keeps its place");
        Check(EnmPickPursuers3(c, 5, 0, out) == 0 && EnmPickPursuers3(c, 5, -2, out) == 0, "pick: no slot left (pursuers on their way hold them): none");
        Check(EnmVictimRangeIni(1500.0) == 1500.0f && EnmVictimRangeIni(50.0) == 200.0f && EnmVictimRangeIni(9999.0) == 3000.0f &&
                  EnmKeepDoorsIni(3) == 3 && EnmKeepDoorsIni(-1) == 0 && EnmKeepDoorsIni(40) == ENM_FOLLOW_MAX && ENM_PURSUE_MISSES == 3,
              "ini: FollowRangeVictims 200..3000 (1500), v3.35's FollowKeepDoors 0..32 (3)");
    }
    // -- the pursuit across doors: the user's log 16:48 (FFFF0007 follows into room 4, then 0 victims at the next door)
    {
        static EnmPursuers P;
        EnmPursuer *e;
        int serial = 10, misses, v, k, okBehind = 1;
        EnmPursueClear(&P);
        Check(EnmPursueAdd(&P, 0xFFFF0007u, 0x0B, 3, 2, serial) == 1 && EnmPursueCount(&P) == 1, "pursue: a victim that follows pursues you");
        ++serial;  // room 4: it arrives (the placeholder, then the real ghost), seen standing
        e = EnmPursueFind(&P, 0xFFFF0007u);
        Check(EnmPursueAbsent(&P, serial) == 1, "pursue: right after the door it is on its way (it holds its FollowMax slot)");
        Check(EnmPursueOnSight(e, serial, 0x0F, 300.0f) == ENM_PURSUE_KEEP && EnmPursueAbsent(&P, serial) == 0 && e->standing,
              "pursue: seen in room 4 (as the placeholder 0F or standing): with you");
        Check(EnmPursueAdd(&P, 0xFFFF0007u, 0x0B, 3, 2, serial) == 0 && EnmPursueCount(&P) == 1 && e->misses == 0,
              "pursue: taken again at the next door (one entry per victim)");
        // it falls behind: three doors without catching up (its record still in an old room)
        misses = 0;
        for (k = 0; k < 2; ++k) okBehind &= EnmPursueAtDoor(&misses, 3, 2, 0x0F, 2, 4, 0.0f) == ENM_PURSUE_KEEP;
        okBehind &= EnmPursueAtDoor(&misses, 3, 2, 0x0F, 2, 4, 0.0f) == ENM_PURSUE_BEHIND && misses == 3;
        Check(okBehind, "pursue: tagged again at 2 doors while on its way; lost at the 3rd door missed (FollowKeepDoors 3)");
        misses = 0;
        Check(EnmPursueAtDoor(&misses, 3, 2, 0x0B, 2, 4, 0.0f) == ENM_PURSUE_PINNED && EnmPursueAtDoor(&misses, 3, 2, 0x0A, 2, 4, 50.0f) == ENM_PURSUE_DOWN &&
                  EnmPursueAtDoor(&misses, 3, 2, -1, -1, -1, 0.0f) == ENM_PURSUE_GONE && EnmPursueAtDoor(&misses, 3, 2, 0x0F, 2, 0x63, 0.0f) == ENM_PURSUE_GONE &&
                  EnmPursueAtDoor(&misses, 3, 2, 0x0F, 3, 4, 0.0f) == ENM_PURSUE_GONE && misses == 0,
              "pursue: its record pinned, down, none, moved out of the world (room 63) or of another world: lost, no door counted");
        v = EnmPursueOnSight(e, serial, 0x0A, 120.0f);
        Check(v == ENM_PURSUE_KEEP && !e->standing, "pursue: a stumble (class 0A, HP > 0) keeps it (it comes once it stands)");
        Check(EnmPursueOnSight(e, serial, 0x0A, 0.0f) == ENM_PURSUE_DOWN && EnmPursueOnSight(e, serial, 9, 0.0f) == ENM_PURSUE_DOWN &&
                  EnmPursueOnSight(e, serial, 0x0B, 0.0f) == ENM_PURSUE_PINNED, "pursue: downed at HP 0 or pinned ends it");
        for (k = 0; k < ENM_PURSUERS_N; ++k) EnmPursueAdd(&P, 0xFFFF0100u + (unsigned)k, 0x0B, 0, 2, serial);
        Check(EnmPursueCount(&P) == ENM_PURSUERS_N && EnmPursueAdd(&P, 0xFFFF0400u, 0x0B, 0, 2, serial) == -1,
              "pursue: at most ENM_PURSUERS_N (256; v3.38 EN338: was 8) per world visit");
        EnmPursueClear(&P);
        Check(EnmPursueCount(&P) == 0 && kEnmPursueWhy[ENM_PURSUE_BEHIND] != 0 && kEnmPursueWhy[ENM_PURSUE_MAX] != 0,
              "pursue: a load / leaving the world / Follow off clears them; every reason has a log name");
    }
    // -- victims' copies in the ledger: a pin is no death
    {
        static EnmCwLedger L;
        unsigned char rec[ENM_REC_SIZE];
        const unsigned c1 = EnmCopyId(9, 2, 1), k1 = EnmCopyKey(2, 0, c1);
        EnmCwClear(&L);
        SetRec(rec, c1, 0x0B, 0x0F, 2, 4, 300.0f);
        EnmCwPut(&L, k1, c1, 4, 0x0B, 3, rec);
        SetRec(rec, c1, 0x0B, 0x0B, 2, 4, 0.0f);
        Check(EnmCwOnRecord2(&L, k1, rec, 1) == 0 && EnmCwFind(&L, k1) && EnmCwFind(&L, k1)->rec[0x26] == 0x0B,
              "ghost copies: a victim's copy pinned in room 4 stays in the ledger, pinned (its Sword lies there)");
        Check(EnmCwOnRecord2(&L, k1, rec, 0) == 1 && !EnmCwFind(&L, k1), "ghost copies: a monster copy's 0x0B is still its death");
        Check(EnmCwOnRecord2(&L, k1, rec, 1) == -1, "ghost copies: not in the ledger: none of its business");
        Check(kEnmWspot[ENM_WSPOT_GHOST] != 0, "ghost copies: put at their record's spot (they pass walls)");
    }
    // -- late copies: the set of victim originals handled in a room visit
    {
        static EnmLateSet S;
        int k, okFull = 1;
        EnmLateReset(&S, 2, 9, 1);
        Check(EnmLateAdd(&S, 0xFFFF0007u) == 1 && EnmLateAdd(&S, 0xFFFF0007u) == 0 && EnmLateHas(&S, 0xFFFF0007u) && !EnmLateHas(&S, 0xFFFF0008u),
              "late: an original handled once per room visit (at the load or when it came in)");
        for (k = 1; k < ENM_LATE_N; ++k) okFull &= EnmLateAdd(&S, 0xFFFF0100u + (unsigned)k) == 1;
        Check(okFull && EnmLateAdd(&S, 0xFFFF0300u) == -1, "late: 32 per room visit (a full set is an anomaly)");
        EnmLateReset(&S, 2, 9, 1);
        Check(!EnmLateHas(&S, 0xFFFF0007u) && S.on, "late: a new room visit starts afresh");
        Check(!EnmLateReady(0x0F) && !EnmLateReady(0x10) && !EnmLateReady(0) && EnmLateReady(1) && EnmLateReady(0x0A) && EnmLateReady(0x0B),
              "late: copies wait for the real ghost (not the follower placeholder 0F); down or pinned ones get them beside them");
    }
    // -- the down countdown: never shorter from a hit, the drain and the pin untouched, at 30 / 60 / 120 / 180 Hz
    {
        static const int kHz[4] = { 30, 60, 120, 180 };
        int h, okDrain = 1, okHit = 1, okHigher = 1, okPin = 1;
        for (h = 0; h < 4; ++h) {
            const float dt = 1.0f / (float)kHz[h];
            float cd = 62.8f, ref = 62.8f, prev = cd;
            int cls = 0x0A, prevCls = 0x0A, tick, kept = 0;
            for (tick = 1; tick <= kHz[h] * 40; ++tick) {
                float k;
                cd -= dt, ref -= dt;  // GhostUpdate's drain
                if (tick == kHz[h] * 1) cd = cd * 0.5f + 3.0f + 0.7f;           // a hit: x 0.5, + 3, + 0..2 (0x47F6xx)
                if (tick == kHz[h] * 5) cd = 30.0f;                              // a re-down rolled lower (0x481870)
                if (tick == kHz[h] * 9) cd = cd * 0.5f + 3.0f;                  // another hit
                k = EnmCdKeep(prevCls, prev, cls, cd, dt);
                if (k > 0.0f) cd = k, ++kept;
                if (tick == kHz[h] * 20) cd += 30.0f, ref += 30.0f;              // a re-roll higher: kept as the game set it
                prev = cd, prevCls = cls;
            }
            okHit &= kept == 3 && fabsf(cd - ref) < 3.0f * dt + 1e-3f;
            cd = 50.0f;
            for (tick = 0; tick < kHz[h] * 10; ++tick) okDrain &= EnmCdKeep(0x0A, cd, 0x0A, cd - dt, dt) == 0.0f, cd -= dt;
            okHigher &= EnmCdKeep(0x0A, 62.8f, 0x0A, 118.9f, dt) == 0.0f;   // the user's log: 62.8 s then 118.9 s
            okPin &= EnmCdKeep(0x0A, 80.0f, 0x0B, 900.0f, dt) == 0.0f && EnmCdKeep(0x0B, 900.0f, 0x0B, 10.0f, dt) == 0.0f &&
                     EnmCdKeep(0x0A, 80.0f, 9, 0.0f, dt) == 0.0f && EnmCdKeep(0x0A, 80.0f, 8, 0.0f, dt) == 0.0f &&
                     EnmCdKeep(0x0A, 0.0f, 0x0A, 0.0f, dt) == 0.0f;
        }
        Check(okHit, "countdown: 3 lowering hits / re-downs in a 62.8 s down are put back: it ends with the time left (within 3 ticks) at 30 / 60 / 120 / 180 Hz");
        Check(okDrain, "countdown: the normal drain never triggers (10 s at each rate)");
        Check(okHigher, "countdown: a higher roll (62.8 -> 118.9 s, the user's log) is the game's: kept");
        Check(okPin && EnmCdGhostType(0x0B) && EnmCdGhostType(0x0F) && EnmCdGhostType(0x11) && !EnmCdGhostType(0x10) && !EnmCdGhostType(0x0A),
              "countdown: the Sword's pin (0B), rising (8), down without a countdown (9) are never touched; ghosts 0B-0F and 11 only");
        Check(EnmCdKeep(0x0A, 10.0f, 0x0A, 9.85f, 0.5f) == 0.0f && EnmCdKeep(0x0A, 10.0f, 0x0A, 9.5f, 0.5f) > 9.89f,
              "countdown: one tick's drain is capped at 0.1 s (a stall never hides a re-roll)");
        {   // the Sword: pinned (0B, 900 s), pulled out: class 0A with the pin's time for a tick, then the down step sets 2..5 s
            EnmCdState S;
            float k1, k2, k3, k4;
            memset(&S, 0, sizeof S);
            EnmCdStep(&S, 0, 0x0A, 80.0f, 1.0f / 60.0f);               // down
            k1 = EnmCdStep(&S, 1, 0x0B, 900.0f, 1.0f / 60.0f);         // pinned (the pin attack's (1..2) x 600)
            k2 = EnmCdStep(&S, 1, 0x0A, 899.9f, 1.0f / 60.0f);         // the Sword pulled out: 0A, the pin's time
            k3 = EnmCdStep(&S, 1, 0x0A, 3.4f, 1.0f / 60.0f);           // the down step 0x63: 2..5 s
            EnmCdStep(&S, 1, 8, 0.0f, 1.0f / 60.0f);                    // it rises
            EnmCdStep(&S, 1, 1, 0.0f, 1.0f / 60.0f);                    // stands
            EnmCdStep(&S, 1, 0x0A, 90.0f, 1.0f / 60.0f);                // a new down
            k4 = EnmCdStep(&S, 1, 0x0A, 40.0f, 1.0f / 60.0f);          // a hit lowers it: kept again
            Check(k1 == 0.0f && k2 == 0.0f && k3 == 0.0f && k4 > 89.9f,
                  "countdown: the short down after the Sword is pulled out is the game's (never kept); the next real down is kept again");
        }
    }
}

// v3.38 EN338: no timing rule keeps a follower out, the pursuit lasts the world visit, spotted at any distance, the story
// ghosts follow (and their guards), no Wheelchair, the space gates, the game's own downs (30 / 60 / 120 / 180 Hz)
static void TestFollow338(void)
{
    const unsigned clr = 0;
    // -- item 8: no Wheelchair in Copies or Follow; v3.37's strays go home
    {
        const unsigned w = EnmScriptId(5, 0, 12, 3);
        Check(!EnmCopyMonster(6) && !EnmFollowMonster(6) && !EnmCopyEligible(6, 0, 0, clr, clr) && !EnmCopyEligible3(6, 0, 0, clr, clr, 1, 1),
              "wheelchair: never copied (Copies x2..x4)");
        Check(!EnmIsWanderer(w, 6, 5, 7, 5, 0) && EnmPursuerWhy(0, 0, 0, 6, 0, 0, 0, 1, 4) == ENM_PUR_OTHER &&
                  EnmPursuerWhy3(0, 0, 1, 1, 6, 0, 0, 0, 0, 1, 4) == ENM_PUR_OTHER && EnmPursuerWhy(1, 0, 1, 6, 0, 0, 0, 0, 0) == ENM_PUR_OTHER,
              "wheelchair: never follows, never a wanderer (its record away from home is no longer replayed)");
        Check(!EnmReviveType(6), "wheelchair: never revived (unchanged)");
        Check(EnmStrayWheelchair(w, 6, 5, 7, 5, 0) && !EnmStrayWheelchair(w, 6, 5, 12, 5, 0) && !EnmStrayWheelchair(w, 3, 5, 7, 5, 0) &&
                  !EnmStrayWheelchair(w, 6, 4, 7, 5, 0) && !EnmStrayWheelchair(w, 6, 5, 7, 5, 1) && !EnmStrayWheelchair(w, 6, 5, 7, 4, 0) &&
                  !EnmStrayWheelchair(0xFFFF0003u, 6, 5, 7, 5, 0) && !EnmStrayWheelchair(EnmCopyId(12, 3, 1), 6, 5, 7, 5, 0) &&
                  !EnmStrayWheelchair(w, 6, 5, 0, 5, 0) && !EnmStrayWheelchair(EnmScriptId(2, 0, 14, 3), 6, 2, 22, 2, 0),
              "wheelchair: a stray = its record in a foreign room of this stage / half (never at home, another type, a ghost or copy id, the train's 14 -> 22)");
    }
    // -- item 7: the story ghosts and their copies follow; the guards
    {
        Check(EnmFollowGhost(0x0C, 0, 1) && EnmFollowGhost(0x0D, 0, 1) && EnmFollowGhost(0x0E, 0, 1) && EnmFollowGhost(0x0F, 0, 1) &&
                  !EnmFollowGhost(0x0C, 0, 0) && EnmFollowGhost(0x0B, 2, 0) && EnmFollowGhost(0x0D, 1, 0) &&
                  !EnmFollowGhost(0x10, 0, 1) && !EnmFollowGhost(0x11, 0, 1) && !EnmFollowGhost(5, 0, 1),
              "story: Cynthia's ghost 0C v0, Jasper 0D v0, Andrew 0E, Richard 0F follow as ghosts (FollowStoryGhosts); never 10, 11, 05");
        // copy, ghost id, victims, story, type, variant, mode word, released, AI state, script id, kill index
        Check(EnmPursuerWhy3(0, 1, 1, 1, 0x0C, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_YES && EnmPursuerWhy3(0, 1, 1, 1, 0x0D, 0, 0x004, 0, 0, 1, 0) == ENM_PUR_YES &&
                  EnmPursuerWhy3(0, 1, 1, 1, 0x0E, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_YES && EnmPursuerWhy3(0, 1, 1, 1, 0x0F, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_YES,
              "story: the four originals pursue (mode 0, or 4 once they arrived through the game's own arrival)");
        Check(EnmPursuerWhy3(1, 0, 1, 1, 0x0E, 0, 0x000, 0, 0, 0, 0) == ENM_PUR_YES && EnmPursuerWhy3(1, 0, 1, 1, 0x0C, 0, 0x000, 0, 0, 0, 0) == ENM_PUR_YES,
              "story: their copies follow as the victims' copies do (v3.37: never)");
        Check(EnmPursuerWhy3(0, 1, 1, 0, 0x0C, 0, 0, 0, 0, 1, 0) == ENM_PUR_OTHER && EnmPursuerWhy3(1, 0, 1, 0, 0x0E, 0, 0, 0, 0, 0, 0) == ENM_PUR_OTHER &&
                  EnmPursuerWhy3(0, 1, 0, 1, 0x0D, 0, 0, 0, 0, 1, 0) == ENM_PUR_OTHER,
              "story: FollowStoryGhosts=0 is v3.37; Enemies Follow = Monsters takes no ghost");
        Check(EnmPursuerWhy3(0, 1, 1, 1, 0x0C, 0, 0x003, 0, 0, 1, 0) == ENM_PUR_EVENT && EnmPursuerWhy3(0, 1, 1, 1, 0x0F, 0, 0x006, 0, 0, 1, 0) == ENM_PUR_EVENT &&
                  EnmPursuerWhy3(1, 0, 1, 1, 0x0B, 0, 0x004, 0, 0, 0, 0) == ENM_PUR_EVENT,
              "story: event spawn modes never (a copy only in mode 0)");
        Check(EnmPursuerWhy3(0, 1, 1, 1, 0x10, 0, 0, 0, 0, 1, 5) == ENM_PUR_OTHER && EnmPursuerWhy3(0, 0, 1, 1, 0x05, 0, 0, 0, 0, 1, 5) == ENM_PUR_OTHER &&
                  EnmPursuerWhy3(0, 0, 1, 1, 0x05, 0, 8, 0, 0, 1, 5) == ENM_PUR_OTHER,
              "story: never Walter / the Bodies (10, even with a ghost id), never the Wall Man or the Wall boss (05 mode 8)");
        Check(EnmPursuerWhy3(0, 1, 1, 1, 0x0B, 3, 0x304, 0, 0, 1, 0) == EnmPursuerWhy2(0, 1, 1, 0x0B, 3, 0x304, 0, 0, 0, 0) &&
                  EnmPursuerWhy3(1, 0, 1, 1, 0x0B, 2, 0x200, 0, 0, 0, 0) == ENM_PUR_YES &&
                  EnmPursuerWhy3(0, 0, 1, 1, 0x0A, 0, 0, 0, 0, 1, 6) == ENM_PUR_YES && EnmPursuerWhy3(0, 0, 1, 1, 0x0A, 0, 0, 0, 0, 1, 0) == ENM_PUR_NOKILL &&
                  EnmPursuerWhy3(1, 0, 1, 1, 0x0A, 0, 6, 0, 0, 0, 0) == ENM_PUR_EVENT && EnmPursuerWhy3(0, 0, 1, 1, 0x11, 0, 0, 0, 0, 1, 3) == ENM_PUR_YES,
              "story: victims, monsters, their copies and the Patient keep v3.37's rules");
        // stage, from, to, flag 2DC, flag 3BF
        Check(EnmStoryFollowWhy(0x0E, 4, 5, 6, 0, 0) == ENM_SF_ANDREW && EnmStoryFollowWhy(0x0E, 4, 5, 6, 1, 0) == ENM_SF_OK &&
                  EnmStoryFollowWhy(0x0E, 5, 5, 6, 0, 0) == ENM_SF_OK && EnmStoryFollowWhy(0x0E, 8, 2, 8, 0, 0) == ENM_SF_OK,
              "guard: Andrew never follows in stage 4 before flag 2DC (0x4CE050 plays his pin event there); elsewhere he does");
        Check(EnmStoryFollowWhy(0x0F, 5, 3, 8, 1, 0) == ENM_SF_RICHARD && EnmStoryFollowWhy(0x0F, 5, 8, 3, 1, 0) == ENM_SF_RICHARD &&
                  EnmStoryFollowWhy(0x0F, 5, 3, 8, 1, 1) == ENM_SF_OK && EnmStoryFollowWhy(0x0F, 5, 8, 16, 0, 0) == ENM_SF_OK &&
                  EnmStoryFollowWhy(0x0F, 6, 3, 8, 0, 0) == ENM_SF_OK,
              "guard: Richard never into or out of room 5:3 before flag 3BF (his 30 s there set it); other rooms and after 3BF he does");
        Check(EnmStoryFollowWhy(0x11, 5, 3, 4, 0, 0) == ENM_SF_RICHARD && EnmStoryFollowWhy(0x11, 5, 4, 3, 0, 0) == ENM_SF_RICHARD &&
                  EnmStoryFollowWhy(0x11, 5, 4, 5, 0, 0) == ENM_SF_OK,
              "guard: the Patient (11) runs the same ScratchThink 0x4BFCB0 (its 30 s in 5:3 set 3BF too): the same guard");
        Check(EnmStoryFollowWhy(0x0C, 4, 3, 13, 0, 0) == ENM_SF_OK && EnmStoryFollowWhy(0x0D, 5, 3, 8, 0, 0) == ENM_SF_OK &&
                  EnmStoryFollowWhy(0x0A, 5, 3, 8, 0, 0) == ENM_SF_OK && kEnmStoryFollowWhy[ENM_SF_ANDREW][0] && kEnmStoryFollowWhy[ENM_SF_RICHARD][0],
              "guard: Cynthia's ghost, Jasper and the monsters have none (the game's 0x464880 decides for ghosts); every refusal has a log text");
    }
    // -- item 4: no timing rule: no list age, no 20 s give-up; the 300-unit / ring placement and the stagger stay
    {
        static const int kHz[4] = { 30, 60, 120, 180 };
        int h, okLate = 1, okOld = 1;
        Check(EnmDoorVerdict2(2, 2, 5, 4, 7, 0, 5) == ENM_DOOR_OK && EnmDoorVerdict(2, 2, 5, 4, 7, 0, 60000, 6000, 5) == ENM_DOOR_STALE,
              "door: a list 60 s old carries (v3.37: 'too old' after 6 s)");
        Check(EnmDoorVerdict2(2, 2, 5, 4, 0, 0, 5) == ENM_DOOR_WARP && EnmDoorVerdict2(5, 5, 5, 4, 7, 1, 5) == ENM_DOOR_LIFT &&
                  EnmDoorVerdict2(3, 2, 5, 4, 7, 0, 5) == ENM_DOOR_OTHERSTAGE && EnmDoorVerdict2(1, 1, 5, 4, 7, 0, 5) == ENM_DOOR_STAGE &&
                  EnmDoorVerdict2(2, 2, 4, 4, 7, 0, 4) == ENM_DOOR_SAMEROOM && EnmDoorVerdict2(2, 2, 5, 4, 7, 0, 9) == ENM_DOOR_FROM,
              "door: real doors only (door 0, a lift, a world change, Room 302 carry nobody: safety, not timing)");
        Check(EnmArriveStep2(0.0f, 100.0f) == 0 && EnmArriveStep2(0.0f, 300.0f) == 1 && EnmArriveStep2(10.0f, 100.0f) == 2 &&
                  EnmArriveStep2(20.0f, 100.0f) == 2 && EnmArriveStep2(600.0f, 100.0f) == 2 && EnmArriveStep2(600.0f, 900.0f) == 1,
              "arrive: in once you are 300 from the door, a ring spot away from you after 10 s, never given up (v3.37: at 20 s)");
        Check(fabsf(EnmFollowDue(31, 3.0f) - 49.5f) < 1e-4f, "arrive: the stagger stays: the 32nd follower is due at 3 + 31 x 1.5 = 49.5 s");
        // a follower due at 3 s that finds no space until 95 s (the room full): it waits and comes in at the first tick after
        for (h = 0; h < 4; ++h) {
            const float dt = 1.0f / (float)kHz[h];
            float T = 0.0f, in = -1.0f, gaveUp = -1.0f;
            int tick;
            for (tick = 0; tick < kHz[h] * 120 && in < 0.0f; ++tick) {
                T += dt;
                if (T < EnmFollowDue(0, 3.0f)) continue;
                if (gaveUp < 0.0f && EnmArriveStep(T - 3.0f, 100.0f) == 3) gaveUp = T;
                if (EnmArriveStep2(T - 3.0f, 100.0f) != 0 && T >= 95.0f) in = T;
            }
            okLate &= in >= 95.0f && in <= 95.0f + dt + 1e-3f;
            okOld &= gaveUp > 22.9f && gaveUp < 23.0f + dt + 1e-3f;
        }
        Check(okLate, "arrive: one that waits 92 s for space comes in within a tick of the space at 30 / 60 / 120 / 180 Hz");
        Check(okOld, "arrive: (v3.37's rule would have given it up at 23 s at every rate)");
        Check(EnmKeepDoorsIni2(-1) == ENM_KEEP_ALWAYS && EnmKeepDoorsIni2(-7) == ENM_KEEP_ALWAYS && EnmKeepDoorsIni2(0) == 0 &&
                  EnmKeepDoorsIni2(3) == 3 && EnmKeepDoorsIni2(99) == ENM_FOLLOW_MAX,
              "ini: FollowKeepDoors missing / Always / < 0 = the whole world visit; 0 and 1..32 keep their old meaning");
        Check(EnmRoomTimerGate(5.0f, 0.0f) == ENM_RT_OK && EnmRoomTimerGate(5.0f, 30.0f) == ENM_RT_WAIT && EnmRoomTimerGate(30.0f, 30.0f) == ENM_RT_OK &&
                  EnmRoomTimerGate(1.0e6f, 1.7014118e38f) == ENM_RT_NEVER && EnmRoomTimerGate(0.0f, 10.0f) == ENM_RT_WAIT,
              "room gate: the game's ghost timer (0x46F9C0: 0, 10 or 30 s; 'never' = 1.7e38) as 0x486150 tests it");
    }
    // -- item 6: spotted at any distance (a per-room-visit mark)
    {
        static EnmSeenSet S;
        int fresh, how;
        EnmSeenReset(&S, 7);
        how = EnmSeenSee(&S, 0x85040003u, 150.0f, 0, 0, &fresh);
        Check(how == 0 && fresh == 0 && S.n == 1, "spotted: a Hummer far away, unhurt, not seeing you: not spotted (it does not follow)");
        how = EnmSeenSee(&S, 0x85040003u, 150.0f, 1, 0, &fresh);
        Check(how == ENM_SEEN_NEAR && fresh == ENM_SEEN_NEAR, "spotted: it came within FollowRange once");
        how = EnmSeenSee(&S, 0x85040003u, 150.0f, 0, 0, &fresh);
        Check(how == ENM_SEEN_NEAR && fresh == 0, "spotted: the mark stays when it flies away again (any distance at the door)");
        how = EnmSeenSee(&S, 0x85040004u, 150.0f, 0, 0, &fresh);
        how = EnmSeenSee(&S, 0x85040004u, 120.0f, 0, 0, &fresh);
        Check(how == ENM_SEEN_HIT && fresh == ENM_SEEN_HIT, "spotted: one hit from afar (HP 150 -> 120: by you, Eileen or a fire) is marked");
        how = EnmSeenSee(&S, 0x85040005u, 80.0f, 0, 0, &fresh);
        how = EnmSeenSee(&S, 0x85040005u, 100.0f, 0, 0, &fresh);
        Check(how == 0, "spotted: HP that grows (healing) is no hit");
        how = EnmSeenSee(&S, 0x85040005u, 95.0f, 0, 0, &fresh);
        Check(how == ENM_SEEN_HIT, "spotted: a fall below the highest HP seen is a hit");
        how = EnmSeenSee(&S, 0x850A0001u, -1.0f, 0, 0, &fresh);
        how = EnmSeenSee(&S, 0x850A0001u, -1.0f, 0, 1, &fresh);
        Check(how == ENM_SEEN_SAW && fresh == ENM_SEEN_SAW, "spotted: a dog whose sense found you (no HP block known: no hit test)");
        {
            int k, full = 0;
            for (k = S.n; k < ENM_SEEN_N; ++k) EnmSeenSee(&S, 0x1E000000u + (unsigned)k, 10.0f, 0, 0, NULL);
            full = EnmSeenSee(&S, 0x1E00F000u, 10.0f, 1, 0, NULL) == -1;
            Check(full && EnmSeenSee(&S, 0x85040003u, 150.0f, 0, 0, NULL) == ENM_SEEN_NEAR,
                  "spotted: 96 enemies a visit (the list walk's guard); the marks already made stay");
        }
        EnmSeenReset(&S, 8);
        Check(EnmSeenSee(&S, 0x85040003u, 150.0f, 0, 0, NULL) == 0, "spotted: a new room visit starts afresh");
        Check(EnmSenseOff(3) == 0x44 && EnmSenseOff(7) == 0x7C && EnmSenseOff(8) == 0x24 && EnmSenseOff(0x0A) == 0x10 &&
                  !EnmSenseOff(6) && !EnmSenseOff(0x11) && !EnmSenseOff(0x0B),
              "sense: the EnemySense record of the Hummer, Gum Head, Twin Victim, Sniffer Dog (decomp headers); none known for others");
        Check(EnmSenseSawPlayer(1, 0, 0) && !EnmSenseSawPlayer(1, 0x01234560u, 0) && !EnmSenseSawPlayer(1, 0, 1) &&
                  !EnmSenseSawPlayer(0, 0, 0) && !EnmSenseSawPlayer(1, 0, 2),
              "sense: found the player (target 0, no object); Eileen (1), Cynthia (2), a scent / object search: no");
    }
    // -- the door: pursuers first, no distance, FollowMax 32, the rest wait
    {
        EnmCand4 c[6];
        int out[8], m, a, b, d;
        c[0].d2 = 5000.0f * 5000.0f, c[0].ok = 1, c[0].prio = 0;   // spotted, now far away
        c[1].d2 = 100.0f * 100.0f, c[1].ok = 1, c[1].prio = 0;     // near
        c[2].d2 = 9000.0f * 9000.0f, c[2].ok = 1, c[2].prio = 1;   // a pursuer far away
        c[3].d2 = 10.0f, c[3].ok = 0, c[3].prio = 1;               // not eligible
        c[4].d2 = 300.0f * 300.0f, c[4].ok = 1, c[4].prio = 1;     // a pursuer near
        c[5].d2 = 2000.0f * 2000.0f, c[5].ok = 1, c[5].prio = 0;
        m = EnmPickPursuers4(c, 6, ENM_PUR_MAX, out);
        Check(m == 5 && out[0] == 4 && out[1] == 2 && out[2] == 1 && out[3] == 5 && out[4] == 0,
              "door: pursuers first (nearest first), then the spotted ones at any distance (5000 away too)");
        Check(EnmPickPursuers4(c, 6, 2, out) == 2 && out[0] == 4 && out[1] == 2 && EnmPickPursuers4(c, 6, 0, out) == 0,
              "door: a cap keeps the pursuers' places");
        EnmDoorSlots(3, 5, 10, 32, &a, &b, &d);
        Check(a == 3 && b == 5 && d == 10, "door: 18 followers under FollowMax 32: all come");
        EnmDoorSlots(3, 5, 10, 6, &a, &b, &d);
        Check(a == 3 && b == 3 && d == 0, "door: FollowMax 6: the room's pursuers, then those away; the newly spotted wait (pursuers now)");
        EnmDoorSlots(40, 9, 9, ENM_FOLLOW_DEFAULT, &a, &b, &d);
        Check(a == 32 && b == 0 && d == 0, "door: never more than FollowMax (32) through one door");
        EnmDoorSlots(3, 5, 10, 0, &a, &b, &d);
        Check(a == 0 && b == 0 && d == 0, "door: no slot: everyone waits");
    }
    // -- item 5: the pursuit lasts the world visit (a downed ghost keeps pursuing)
    {
        static EnmPursuers P;
        EnmPursuer *e;
        int misses = 0, k, ok = 1;
        EnmPursueClear(&P);
        Check(EnmPursueAdd2(&P, 0xFFFF0007u, 0x0B, 3, 2, 4, 0, 1) == 1 && (e = EnmPursueFind(&P, 0xFFFF0007u)) != 0 && e->ghost && !e->copy,
              "pursue: a victim pursues (ghost)");
        Check(EnmPursueOnSight2(e, 5, 0x0A, 0) == ENM_PURSUE_KEEP && !e->standing && e->seen == 5 &&
                  EnmPursueOnSight2(e, 5, 9, 0) == ENM_PURSUE_KEEP,
              "pursue: a downed ghost (class 0A / 09, HP 0 too) keeps pursuing: the game decides when it gets up (v3.35: ended)");
        Check(EnmPursueOnSight2(e, 5, 0x0B, 0) == ENM_PURSUE_PINNED, "pursue: a ghost's pin ends it (it stays with its Sword)");
        EnmPursueAdd2(&P, 0x85040003u, 0x03, 0, 5, 4, 0, 0);
        e = EnmPursueFind(&P, 0x85040003u);
        Check(EnmPursueOnSight2(e, 5, 1, 0) == ENM_PURSUE_KEEP && e->standing && EnmPursueOnSight2(e, 5, 0x0B, 0) == ENM_PURSUE_DIED &&
                  EnmPursueOnSight2(e, 5, 3, 1) == ENM_PURSUE_DIED,
              "pursue: a monster's death (class 0B, or EnemyIsDead) ends it");
        for (k = 0; k < 100; ++k) ok &= EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, 0x0F, 2, 4) == ENM_PURSUE_KEEP;
        Check(ok && misses == 0, "pursue: 100 doors away and still pursuing (FollowKeepDoors Always; v3.35: lost after 3)");
        Check(EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, 0x0A, 2, 4) == ENM_PURSUE_KEEP &&
                  EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, 0x0B, 2, 4) == ENM_PURSUE_PINNED &&
                  EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 0, 0x0B, 2, 4) == ENM_PURSUE_DIED &&
                  EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, -1, -1, -1) == ENM_PURSUE_GONE &&
                  EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, 0x0F, 2, 0x63) == ENM_PURSUE_GONE &&
                  EnmPursueAtDoor2(&misses, ENM_KEEP_ALWAYS, 2, 1, 0x0F, 3, 4) == ENM_PURSUE_GONE,
              "pursue: away it ends only pinned, killed, or gone from this world (down records keep it)");
        misses = 0, ok = 1;
        for (k = 0; k < 2; ++k) ok &= EnmPursueAtDoor2(&misses, 3, 2, 1, 0x0F, 2, 4) == ENM_PURSUE_KEEP;
        Check(ok && EnmPursueAtDoor2(&misses, 3, 2, 1, 0x0F, 2, 4) == ENM_PURSUE_BEHIND, "pursue: FollowKeepDoors=3 restores v3.35's door count");
        Check(EnmPursueAdd(&P, 0xFFFF0009u, 0x0D, 1, 2, 4) == 1 && EnmPursueFind(&P, 0xFFFF0009u)->ghost == 1 &&
                  EnmPursueAdd2(&P, EnmCopyId(5, 2, 1), 0x0A, 0, 2, 4, 1, 0) == 1 && EnmPursueFind(&P, EnmCopyId(5, 2, 1))->copy == 1,
              "pursue: one table for victims, story ghosts, monsters and copies");
        Check(ENM_PURSUERS_N >= 64 + ENM_CW_N && ENM_CW_N == 128 && ENM_FOLLOW_MAX == 32 && ENM_PUR_MAX >= 40 &&
                  kEnmPursueWhy[ENM_PURSUE_DIED] && kEnmPursueWhy[ENM_PURSUE_DENIED] && kEnmPursueWhy[ENM_PURSUE_HOME],
              "caps: 32 followers a door, a 128-entry ledger, a pursuer table for 64 records + the ledger, every reason has a log text");
    }
    // -- item 5: the space gates; a copy's kept state needs a record slot
    {
        EnmSpace s;
        s.listOk = s.tasksOk = s.actorsOk = s.bonesOk = 1, s.hitFree = 200, s.ikFree = 400, s.recFree = 2;
        Check(EnmSpaceWhy(&s, 1) == ENM_SPACE_OK, "space: a room with room for it: it comes");
        s.hitFree = 23;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_HITS, "space: 23 hit shapes free (24 kept): it waits");
        s.hitFree = 24, s.ikFree = 63;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_IK, "space: 63 IK handles free (64 kept): it waits");
        s.ikFree = -1, s.hitFree = -1;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_HITS, "space: a pool count not known yet: it waits (v3.38 REV338)");
        s.hitFree = 24;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_IK, "space: the IK count not known: it waits");
        s.ikFree = 64;
        s.recFree = 0;
        Check(EnmSpaceWhy(&s, 1) == ENM_SPACE_RECORD && EnmSpaceWhy(&s, 0) == ENM_SPACE_OK,
              "space: all 64 enemy records in use: a copy with a kept state waits; one without comes fresh");
        s.listOk = 0, s.tasksOk = 0;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_LIST, "space: the first gate that fails is the reason (list, tasks, actors, bones, hits, IK, record)");
        s.listOk = 1;
        Check(EnmSpaceWhy(&s, 0) == ENM_SPACE_TASKS && kEnmSpaceWhy[ENM_SPACE_RECORD] && kEnmSpaceWhy[ENM_SPACE_IK], "space: every reason has a log text");
        Check(!EnmCopyNeedsRecord(1, 170.0f, 170.0f) && EnmCopyNeedsRecord(1, 120.0f, 170.0f) && EnmCopyNeedsRecord(0x0A, 170.0f, 170.0f) &&
                  EnmCopyNeedsRecord(0x0B, 300.0f, 300.0f) && !EnmCopyNeedsRecord(0, 0.0f, 170.0f),
              "space: a copy needs a record slot only for a state a fresh copy would lose (hurt, down, pinned)");
        {   // a pack of 32 dogs (38 bones) into a full Subway 2:2 with the Auto pool: the bone budget lets part in, the rest wait
            int k, freeRec = 3899 - 9 * 38, head = 12 * EnmHead(0x0A, 38), fit = 0;
            for (k = 0; k < 32; ++k)
                if (EnmFits(freeRec, head, EnmCost(0x0A, 0), 4096)) ++fit, freeRec -= 38, head += EnmHead(0x0A, 38);
            printf("      (32 dogs into room 2:2 with the Auto pool: %d fit the bone budget now, %d wait)\n", fit, 32 - fit);
            Check(fit >= 8 && fit < 32, "space: a pack of 32: the budget admits part now (more than v3.37's 8), the rest wait, none dropped");
        }
    }
    // -- items 9 / 10: the game's own down countdown by default; a ghost copy on its way rises by the game's clock
    {
        float cd = 40.0f, hp = 0.0f;
        Check(ENM_DOWNKEEP_DEFAULT == 0, "downs: DownKeep defaults to 0 (vanilla: a hit halves the countdown; no one-tick dip of the bar)");
        Check(EnmCdKeep(0x0A, 99.8f, 0x0A, 53.0f, 1.0f / 60.0f) > 99.7f, "downs: the hidden DownKeep=1 still keeps a countdown a hit lowered");
        EnmGhostDecay(&cd, &hp, 700.0f, 2, 20.0f);
        Check(fabsf(cd - 20.0f) < 1e-3f && hp == 0.0f, "downs: a downed ghost copy on its way: 20 s later 20 s left (it waits, lying down)");
        EnmGhostDecay(&cd, &hp, 700.0f, 2, 21.0f);
        Check(cd == 0.0f && fabsf(hp - 4.0f) < 1e-3f, "downs: then it is up (countdown 0, HP regained at 2^difficulty per second) and comes");
    }
}

// v3.38 REV338 (inspection 3): R1 the story ghosts' originals are ghost-id ghosts (the ghosts' path); R2 a tagged ghost
// passes the gates on the stock "yes" too
static void TestRev338_3(void)
{
    // the stock placements (tools/em_script_dump.py): Cynthia 0C v0 FFFF0002, Jasper 0D v0 FFFF0003, Andrew 0E FFFF0004,
    // Richard 0F FFFF0005; EnemyAllocWorkStruct 0x45CA70 makes the override the spawn id
    static const struct { int type, variant; unsigned id; } kStory[4] = {
        { 0x0C, 0, 0xFFFF0002u }, { 0x0D, 0, 0xFFFF0003u }, { 0x0E, 0, 0xFFFF0004u }, { 0x0F, 0, 0xFFFF0005u } };
    int i, ok = 1;
    for (i = 0; i < 4; ++i) {
        const int gk = EnmGhostKind(kStory[i].type, 0, kStory[i].id);
        ok &= gk && !EnmStoryNoGhostId(kStory[i].type, kStory[i].variant, 0, kStory[i].id);
        ok &= EnmPursuerWhy3(0, EnmIsGhostId(kStory[i].id), 1, 1, kStory[i].type, kStory[i].variant, 0x000, 0, 0,
                             EnmIsScriptId(kStory[i].id), 0) == ENM_PUR_YES;
        ok &= EnmPursuerWhy3(0, EnmIsGhostId(kStory[i].id), 1, 1, kStory[i].type, kStory[i].variant, 0x004, 0, 0,
                             EnmIsScriptId(kStory[i].id), 0) == ENM_PUR_YES;  // (arrived: mode 4)
        ok &= EnmPursuerWhy3(0, EnmIsGhostId(kStory[i].id), 1, 0, kStory[i].type, kStory[i].variant, 0, 0, 0,
                             EnmIsScriptId(kStory[i].id), 0) == ENM_PUR_OTHER;  // FollowStoryGhosts=0
    }
    Check(ok, "R1: the four story ghosts with their stock ids are ghost-path candidates and pursue (mode 0 / 4); FollowStoryGhosts=0 keeps them out");
    Check(EnmGhostKind(0x0B, 0, 0xFFFF0007u) && EnmGhostKind(0x0C, 1, EnmCopyId(8, 3, 1)) && !EnmGhostKind(0x0A, 0, 0xFFFF0003u) &&
              !EnmGhostKind(0x11, 0, EnmScriptId(5, 0, 3, 1)) && !EnmGhostKind(0x10, 0, 0xFFFF000Eu),
          "R1: ghost kind = a ghost type (0B..0F) with a ghost id or a copy id; never the Patient (11), Walter (10), a monster");
    Check(EnmStoryNoGhostId(0x0D, 0, 0, EnmScriptId(3, 1, 6, 1)) && !EnmGhostKind(0x0D, 0, EnmScriptId(3, 1, 6, 1)) &&
              !EnmStoryNoGhostId(0x0D, 1, 0, EnmScriptId(3, 1, 6, 1)) && !EnmStoryNoGhostId(0x0E, 0, 1, EnmCopyId(32, 4, 1)),
          "R1: a story ghost without a ghost id (no stock script makes one) is flagged as an anomaly, not followed");
    // stock yes, denied, tagged, story ok, timer ok, tick free, space ok
    Check(EnmGhostAdmit(1, 0, 0, 0, 0, 0, 0) == ENM_GA_STOCK && EnmGhostAdmit(0, 1, 0, 1, 1, 1, 1) == ENM_GA_STOCK,
          "R2: an untagged ghost always gets the stock answer");
    Check(EnmGhostAdmit(1, 0, 1, 1, 1, 1, 0) == ENM_GA_REFUSE && EnmGhostAdmit(1, 0, 1, 1, 1, 0, 1) == ENM_GA_REFUSE &&
              EnmGhostAdmit(1, 0, 1, 0, 1, 1, 1) == ENM_GA_REFUSE,
          "R2: a tagged ghost the stock rule lets in still waits for space, its tick and the story guard (v3.38 inspection 3: it skipped them)");
    Check(EnmGhostAdmit(1, 0, 1, 1, 1, 1, 1) == ENM_GA_ADMIT && EnmGhostAdmit(0, 0, 1, 1, 1, 1, 1) == ENM_GA_ADMIT,
          "R2: with space, its tick free and the guard passed it comes in on a stock yes or a plain stock no");
    Check(EnmGhostAdmit(0, 1, 1, 1, 1, 1, 1) == ENM_GA_DENIED && EnmGhostAdmit(0, 1, 1, 0, 0, 0, 0) == ENM_GA_DENIED,
          "R2: the game's story refusal (denied after a no) is never overridden: it ends the pursuit");
    Check(EnmGhostAdmit(1, 0, 1, 1, 0, 1, 1) == ENM_GA_STOCK && EnmGhostAdmit(0, 0, 1, 1, 0, 1, 1) == ENM_GA_STOCK,
          "R2: the room timer not run out: the stock answer (the caller's own timer test keeps it out; no tick is used)");
}

static void TestHpSafe3351(void)  // v3.35.1 HF3351
{
    static const float blk[2] = { 250.0f, 300.0f };
    Check(EnmHpFromBlock(NULL, 1.0f) == 1.0f && EnmHpFromBlock(NULL, -1.0f) == -1.0f,
          "hp: no HP block (a victim just re-made after following) gives the fallback, never a read through NULL");
    Check(EnmHpFromBlock((const float *)(size_t)0x34, 1.0f) == 1.0f, "hp: a pointer below 0x10000 gives the fallback");
    Check(EnmHpFromBlock(blk, 1.0f) == 250.0f, "hp: a real block gives its HP");
    Check(EnmPursueOnSight != NULL, "hp: the pursuer rule is linked");
}

// v3.41 EN341: ghost copies in another room are made the way the game's own ghosts arrive (no record on a plain "no")
static void TestGhostCopy341(void)
{
    static const int states[5] = { 0, 0x01, 0x0A, 0x0B, 0x0F };
    static const float cds[4] = { 0.0f, -1.0f, 0.5f, 45.0f };
    int si, ci, yes, den, half, bad = 0, badModel = 0;
    float nan;
    memset(&nan, 0xFF, sizeof nan);  // (a quiet NaN)
    // the v3.40 log's case: a standing victim copy (record state 0F, countdown 0) into stage 2 room 4 (rule: no, not denied)
    Check(EnmGhostGateRefuses(1, 0, 1, 0, 0.0f) == 1 && EnmGhostGateRefuses(1, 0, 0, 0, 0.0f) == 0,
          "341: the v3.40 arrival with its record where the rule says no: refused when Reconcile keeps the record (half), made "
          "fresh when it deletes it");
    Check(EnmGhostGateRefuses(1, 0, 0, 1, 0.0f) == 1 && EnmGhostGateRefuses(1, 0, 0, 0, 10.0f) == 1,
          "341: with its record pinned or down where the rule says no: always refused (Reconcile keeps those records)");
    Check(EnmGhostGateRefuses(0, 0, 1, 0, 0.0f) == 0 && EnmGhostGateRefuses(0, 0, 1, 1, 30.0f) == 0,
          "341: no record of its id in the table: the gate never asks the room rule (the late copies were made so)");
    Check(EnmGhostGateRefuses(1, 1, 1, 1, 30.0f) == 0, "341: the rule says yes: the record goes in (its state restored)");
    Check(EnmGhostCopyMake(0, 0, 0x0F, 0.0f) == ENM_GCM_FRESH, "341: the log's case is made fresh (no record)");
    Check(EnmGhostCopyMake(0, 1, 0x0F, 0.0f) == ENM_GCM_DENIED && EnmGhostCopyMake(0, 1, 0x0B, 0.0f) == ENM_GCM_DENIED,
          "341: a story flag's refusal is never overridden");
    Check(EnmGhostCopyMake(1, 0, 0x0B, 0.0f) == ENM_GCM_RECORD && EnmGhostCopyMake(1, 1, 0x0A, 9.0f) == ENM_GCM_RECORD,
          "341: the rule says yes: with its record (pinned / down kept), as v3.40");
    Check(EnmGhostCopyMake(0, 0, 0x0B, 0.0f) == ENM_GCM_LATER && EnmGhostCopyMake(0, 0, 0x0A, 12.0f) == ENM_GCM_LATER,
          "341: pinned or down where the rule says no: not now (never stood up fresh)");
    Check(EnmGhostCopyMake(0, 0, 0x0F, nan) == ENM_GCM_LATER, "341: a countdown that is not a number: not now");
    // every case: the way chosen is never one the game refuses (keepHalf both ways), and never loses a pin or a down
    for (si = 0; si < 5; ++si)
        for (ci = 0; ci < 4; ++ci)
            for (yes = 0; yes < 2; ++yes)
                for (den = 0; den < 2; ++den)
                    for (half = 0; half < 2; ++half) {
                        const int st = states[si], how = EnmGhostCopyMake(yes, den, st, cds[ci]);
                        const int pinned = st == 0x0B, down = cds[ci] > 0.0f;
                        if (how < 0 || how >= ENM_GCM_N) ++bad;
                        if (how == ENM_GCM_RECORD && EnmGhostGateRefuses(1, yes, half, pinned, cds[ci])) ++badModel;
                        if (how == ENM_GCM_FRESH && (EnmGhostGateRefuses(0, yes, half, pinned, cds[ci]) || pinned || down)) ++badModel;
                        if (how == ENM_GCM_FRESH && yes) ++bad;          // (a yes keeps its record: its HP as it was)
                        if (how == ENM_GCM_DENIED && (yes || !den)) ++bad;
                        if (!yes && !den && !pinned && !down && how != ENM_GCM_FRESH) ++bad;
                    }
    Check(!bad, "341: the decision table (yes -> record, denied -> home, pinned / down on a no -> later, else fresh)");
    Check(!badModel, "341: no chosen way is one the game's create refuses (EnmGhostGateRefuses, both Reconcile draws)");
    // the placement's story flags (EnemySpawnCheckStoryFlags 0x463FF0)
    Check(EnmPlacementFlagsOk(0, 0, 0, 0) == 1 && EnmPlacementFlagsOk(0x13A, 1, 0, 0) == 1 && EnmPlacementFlagsOk(0, 0, 0x13A, 0) == 1,
          "341: flags: none, a needed one set, a forbidden one clear: ok");
    Check(EnmPlacementFlagsOk(0x13A, 0, 0, 0) == 0 && EnmPlacementFlagsOk(0, 0, 0x13A, 1) == 0,
          "341: flags: a needed one clear or a forbidden one set: refused (the game's rule, no ANOMALY)");
    Check(EnmPlacementFlagsOk(-1, 0, 0, 0) == -1 && EnmPlacementFlagsOk(0, 0, ENM_STORY_FLAGS_N, 1) == -1 &&
          EnmPlacementFlagsOk(ENM_STORY_FLAGS_N - 1, 1, 0, 0) == 1,
          "341: flags: a number outside gStoryFlags (0x140 bytes) is not judged");
}
// v3.49 EF349: Enemies Follow's four values, Victims Only's pursuer rule, the ledger's monster copies
static void TestFollowValues349(void)
{
    static const int kTypes[] = { 0x02, 0x03, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11 };
    static const unsigned kModes[] = { 0x000, 0x004, 0x003, 0x100, 0x104, 0x006 };
    static EnmCwLedger l;
    static unsigned char rec[ENM_REC_SIZE];
    int m, ti, mi, copy, gid, vic, story, variant, scr, kill, bad = 0, badOn = 0, monYes = 0, ghostYes = 0;
    // the values (gameplay.inc kGpName[GP_FOLLOW]: Off, Monsters, Victims Only, Monsters + Victims)
    Check(ENM_FOLLOW_OFF == 0 && ENM_FOLLOW_MONSTERS == 1 && ENM_FOLLOW_VICTIMS == 2 && ENM_FOLLOW_BOTH == 3 && ENM_FOLLOW_N == 4,
          "349: the four values in the row's order");
    Check(!EnmFollowValueOn(ENM_FOLLOW_OFF) && EnmFollowValueOn(ENM_FOLLOW_MONSTERS) && EnmFollowValueOn(ENM_FOLLOW_VICTIMS) &&
          EnmFollowValueOn(ENM_FOLLOW_BOTH) && !EnmFollowValueOn(-1) && !EnmFollowValueOn(4),
          "349: Follow is live for Monsters, Victims Only, Monsters + Victims (never Off or a value outside 0..3)");
    Check(!EnmFollowTakesMonsters(0) && EnmFollowTakesMonsters(1) && !EnmFollowTakesMonsters(2) && EnmFollowTakesMonsters(3) &&
          !EnmFollowTakesMonsters(4) && !EnmFollowTakesMonsters(-1),
          "349: monsters follow with Monsters and Monsters + Victims only");
    Check(!EnmFollowTakesVictims(0) && !EnmFollowTakesVictims(1) && EnmFollowTakesVictims(2) && EnmFollowTakesVictims(3) &&
          !EnmFollowTakesVictims(4) && !EnmFollowTakesVictims(-1),
          "349: the victims' ghosts follow with Victims Only and Monsters + Victims only");
    // EnmPursuerWhy4: with monsters it is EnmPursuerWhy3; without, only the ghosts' path is left (and it is EnmPursuerWhy3's)
    for (ti = 0; ti < (int)(sizeof kTypes / sizeof kTypes[0]); ++ti)
        for (mi = 0; mi < (int)(sizeof kModes / sizeof kModes[0]); ++mi)
            for (copy = 0; copy < 2; ++copy)
                for (gid = 0; gid < 2; ++gid)
                    for (vic = 0; vic < 2; ++vic)
                        for (story = 0; story < 2; ++story)
                            for (variant = 0; variant < 2; ++variant)
                                for (scr = 0; scr < 2; ++scr)
                                    for (kill = 0; kill < 2; ++kill) {
                                        const int t = kTypes[ti];
                                        const unsigned mw = kModes[mi] | (unsigned)variant << 8;
                                        const int w3 = EnmPursuerWhy3(copy, gid, vic, story, t, variant, mw, 1, 0, scr, kill ? 4 : 0);
                                        const int on = EnmPursuerWhy4(1, copy, gid, vic, story, t, variant, mw, 1, 0, scr, kill ? 4 : 0);
                                        const int vo = EnmPursuerWhy4(0, copy, gid, vic, story, t, variant, mw, 1, 0, scr, kill ? 4 : 0);
                                        const int ghostPath = EnmGhostType(t) && (copy || gid);
                                        if (on != w3) ++badOn;
                                        if (vo != (ghostPath ? w3 : ENM_PUR_OTHER)) ++bad;
                                        if (!ghostPath && w3 == ENM_PUR_YES) ++monYes;
                                        if (ghostPath && vo == ENM_PUR_YES) ++ghostYes;
                                    }
    Check(!badOn, "349: with monsters (Monsters, Monsters + Victims) the rule is v3.48's EnmPursuerWhy3 in every case");
    Check(!bad, "349: Victims Only: every monster and monster copy is of no following kind; every ghost case as v3.48");
    Check(monYes > 0 && ghostYes > 0, "349: the sweep has monsters v3.48 lets follow and ghosts Victims Only lets follow");
    Check(EnmPursuerWhy4(0, 0, 0, 1, 1, 0x03, 0, 0x000, 1, 0, 1, 4) == ENM_PUR_OTHER &&
          EnmPursuerWhy4(1, 0, 0, 1, 1, 0x03, 0, 0x000, 1, 0, 1, 4) == EnmPursuerWhy3(0, 0, 1, 1, 0x03, 0, 0x000, 1, 0, 1, 4),
          "349: a Straggler (03) with its script id and kill index: stays with Victims Only, as v3.48 otherwise");
    Check(EnmPursuerWhy4(0, 0, 1, 1, 1, 0x0B, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_YES &&
          EnmPursuerWhy4(0, 0, 1, 1, 1, 0x0B, 0, 0x004, 0, 0, 1, 0) == ENM_PUR_YES &&
          EnmPursuerWhy4(0, 1, 0, 1, 1, 0x0B, 0, 0x000, 0, 0, 0, 0) == ENM_PUR_YES,
          "349: Victims Only: a victim (0B, placed or arrived) and a victim's copy follow");
    Check(EnmPursuerWhy4(0, 0, 1, 0, 1, 0x0B, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_OTHER,
          "349: Victims Only without the ghosts' site (victims 0): nobody follows");
    Check(EnmPursuerWhy4(0, 0, 0, 1, 1, 0x08, 0, 0x000, 1, 0, 1, 4) == ENM_PUR_OTHER &&
          EnmPursuerWhy4(0, 0, 0, 1, 1, 0x11, 0, 0x000, 1, 0, 1, 4) == ENM_PUR_OTHER,
          "349: Victims Only: the Twin Victims (08) and the Patient (11) are monsters: they stay");
    Check(EnmPursuerWhy4(0, 0, 1, 1, 1, 0x0E, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_YES &&
          EnmPursuerWhy4(0, 0, 1, 1, 0, 0x0E, 0, 0x000, 0, 0, 1, 0) == ENM_PUR_OTHER,
          "349: Victims Only: a story ghost (Andrew 0E) follows as with Monsters + Victims (FollowStoryGhosts)");
    // EnmCwDropMonsters: the monsters' copies leave the ledger, the ghost copies stay
    EnmCwClear(&l);
    memset(rec, 0, sizeof rec);
    for (m = 0; m < 6; ++m) {
        static const int kT[6] = { 0x03, 0x0B, 0x07, 0x0C, 0x0A, 0x0E };
        EnmCwPut(&l, 0x100u + (unsigned)m, 0x1E000000u | (unsigned)m << 2 | 1u, 5, kT[m], 0, rec);
    }
    Check(EnmCwCount(&l) == 6, "349: the ledger holds 3 monster copies and 3 ghost copies");
    Check(EnmCwDropMonsters(&l) == 3 && EnmCwCount(&l) == 3 && EnmCwFind(&l, 0x101u) && EnmCwFind(&l, 0x103u) &&
          EnmCwFind(&l, 0x105u) && !EnmCwFind(&l, 0x100u) && !EnmCwFind(&l, 0x102u) && !EnmCwFind(&l, 0x104u),
          "349: Victims Only at a room load: the 3 monster copies go home, the 3 ghost copies stay");
    Check(EnmCwDropMonsters(&l) == 0 && EnmCwCount(&l) == 3, "349: a second room load drops nothing more");
    EnmCwClear(&l);
    Check(EnmCwDropMonsters(&l) == 0, "349: an empty ledger: nothing");
}
// v3.58 EN358: the randomizer's victims (a monster placement made a victim's ghost: its script id) take the ghosts' path
static void TestDrawnGhosts(void)
{
    const unsigned drawn = EnmScriptId(2, 0, 2, 2), drawnHalf = EnmScriptId(5, 1, 11, 9), stock = 0xFFFF0009u;
    const unsigned copy = EnmCopyId(2, 2, 1);
    int v;
    Check(EnmDrawnGhost(0x0B, 1, 0, drawn), "drawn 0B v1 (script id 02020002) is a drawn victim");
    Check(EnmDrawnGhost(0x0C, 1, 0, drawnHalf) && EnmDrawnGhost(0x0D, 1, 0, drawnHalf), "drawn 0C v1 / 0D v1 (second half)");
    for (v = 0; v < 4; ++v) Check(EnmDrawnGhost(0x0B, v, 0, drawn), "0B every variant");
    Check(!EnmDrawnGhost(0x0C, 0, 0, drawn) && !EnmDrawnGhost(0x0D, 0, 0, drawn), "the story ghosts (0C v0 / 0D v0) are never drawn ones");
    Check(!EnmDrawnGhost(0x0E, 0, 0, drawn) && !EnmDrawnGhost(0x0F, 0, 0, drawn), "Andrew / Richard are never drawn ones");
    Check(!EnmDrawnGhost(0x0B, 1, 0, stock), "a stock victim (FFFF00nn) is not a drawn one");
    Check(!EnmDrawnGhost(0x0B, 1, 1, copy) && !EnmDrawnGhost(0x0B, 1, 0, copy), "a copy is not a drawn one");
    Check(!EnmDrawnGhost(0x0A, 0, 0, drawn) && !EnmDrawnGhost(0x07, 1, 0, drawn), "monsters are not");
    Check(!EnmDrawnGhost(0x0B, 1, 0, 0u), "no id: not");
    Check(EnmGhostPath(0x0B, 1, 0, stock) && EnmGhostPath(0x0B, 1, 0, drawn) && EnmGhostPath(0x0B, 1, 1, copy), "the ghosts' path");
    Check(!EnmGhostPath(0x0A, 0, 0, drawn) && !EnmGhostPath(0x0A, 0, 1, copy), "monsters and their copies: not the ghosts' path");
    // EnmTakePursuers' verdict: v3.57 passed ghostId = EnmIsGhostId(id) only (a drawn victim was ENM_PUR_OTHER)
    Check(EnmPursuerWhy4(1, 0, EnmIsGhostId(drawn), 1, 1, 0x0B, 1, 0x100, 0, 0, 1, 3) == ENM_PUR_OTHER, "v3.57: drawn = of no following kind");
    Check(EnmPursuerWhy4(1, 0, EnmIsGhostId(drawn) || EnmDrawnGhost(0x0B, 1, 0, drawn), 1, 1, 0x0B, 1, 0x100, 0, 0, 1, 3) == ENM_PUR_YES,
          "v3.58: drawn victim follows (Monsters + Victims)");
    Check(EnmPursuerWhy4(0, 0, 1, 1, 1, 0x0B, 1, 0x100, 0, 0, 1, 3) == ENM_PUR_YES, "v3.58: drawn victim follows (Victims Only)");
    Check(EnmPursuerWhy4(1, 0, 1, 0, 1, 0x0B, 1, 0x100, 0, 0, 1, 3) == ENM_PUR_OTHER, "Monsters only: a drawn victim does not follow");
    Check(EnmPursuerWhy4(1, 0, 1, 1, 1, 0x0B, 1, 0x106, 0, 0, 1, 3) == ENM_PUR_EVENT, "a victim in an event mode does not");
    {   // the door's per-room line: came in / still arriving / never in the room
        const unsigned tag[5] = { 0xFFFF0002u, drawn, 0xFFFF0009u, 0u, drawnHalf };
        unsigned char seen[5] = { 0, 0, 0, 0, 0 };
        EnmTagSum s;
        seen[0] |= (unsigned char)EnmTagSeenBits(0x0D), seen[0] |= (unsigned char)EnmTagSeenBits(1);
        seen[1] |= (unsigned char)EnmTagSeenBits(0x0F);
        seen[4] |= (unsigned char)EnmTagSeenBits(0x0A);
        s = EnmTagSummary(tag, seen, 5);
        Check(s.tagged == 4 && s.in == 2 && s.arriving == 1 && s.outside == 1, "tag summary 4 tagged: 2 in, 1 arriving, 1 outside");
        Check(EnmTagSeenBits(0) == ENM_TAG_ARRIVING && EnmTagSeenBits(0x0E) == ENM_TAG_ARRIVING && EnmTagSeenBits(0x0C) == ENM_TAG_IN &&
              EnmTagSeenBits(0x12) == ENM_TAG_IN && EnmTagSeenBits(0x0B) == ENM_TAG_IN, "seen bits by class");
        s = EnmTagSummary(tag, seen, 0);
        Check(s.tagged == 0 && s.in == 0, "no tags");
    }
}

int main(void)
{
    TestFollowValues349();  // v3.49 EF349
    TestGhostCopy341();  // v3.41 EN341
    TestFollow338();  // v3.38 EN338
    TestRev338_3();  // v3.38 REV338 (inspection 3)
    TestHpSafe3351();  // v3.35.1 HF3351
    TestRevive334();  // v3.34 EN334
    TestGhosts335();  // v3.35 GH335
    TestCopyPursuers();
    TestCopyLedger();
    TestWanderSpot();
    TestEventDogs();  // v3.33 EN333
    TestFollow333();
    TestIds();
    TestTypes();
    TestBudget();
    TestRing();
    TestFollow();
    TestSpots();
    TestPoolAtRoom();  // v3.30 EN330
    TestFollowModes();
    TestDoorVerdict();
    TestKillBits();
    TestRevive();
    TestSpotTiers();
    TestQueueRates();
    TestStoryGhosts();  // v3.31 EN331
    TestBeside();
    TestGhostDecay();
    TestClock();
    TestSnapStore();
    TestFlyer();
    TestDrawnGhosts();  // v3.58 EN358
    printf("%s: %d checks, %d failed\n", g_fail ? "FAIL" : "ok", g_n, g_fail);
    return g_fail ? 1 : 0;
}
