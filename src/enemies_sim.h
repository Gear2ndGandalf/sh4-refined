// ================================================================ v3.31 ENEMIES: the pure rules of Enemy Copies and Enemies Follow
// (v3.30 EN330: + the pool at a room change, follow modes, the door verdict, kill bits, Revive Enemies, spot tiers: below)
// (v3.35 GH335: + victims pursue, ghost copies follow, late copies, the down countdown kept: at the end)
// No game memory, no Windows: included by src/enemies.inc and by test/enemies_sim_test.c (test/build_enemies_test.cmd).
// Plan: staging/special_mods/r5_enemies/PLAN.md; RE: re_enemies.md, ../re_special.md B.
#ifndef ENEMIES_SIM_H
#define ENEMIES_SIM_H
#include <math.h>

// ---------------------------------------------------------------- ids
// A room script id (0x52CA00): stage << 24 | room << 16 | n (the object number of the room), | 0x80000000 in the second
// half of the game. A copy's id: 0x1E000000 | room << 10 | n << 2 | k (k = 1..3). Bits 16..23 stay 0, so it is no
// script id (0x52C8D0: room byte 0), no FFFF00nn ghost id, and the automatic id 0x463AA0 skips it (stage bits non-zero).
#define ENM_COPY_TAG 0x1Eu
#define ENM_COPY_ROOMS 64   // rooms 1..63 (the stock scripts use up to 42: tools/em_script_dump.py)
#define ENM_COPY_NS 256     // object numbers 0..255 (the stock scripts use up to 30)
#define ENM_COPY_MAXK 3     // x4 = 3 copies of each original
static int EnmScriptStage(unsigned id) { return (int)(id >> 24 & 0x1F); }
static int EnmScriptRoom(unsigned id) { return (int)(id >> 16 & 0xFF); }
static int EnmScriptN(unsigned id) { return (int)(id & 0xFFFF); }
static int EnmScriptHalf(unsigned id) { return (int)(id >> 31); }
static int EnmIsScriptId(unsigned id) { return EnmScriptStage(id) > 0 && EnmScriptRoom(id) > 0; }  // as 0x52C8D0
static int EnmIsGhostId(unsigned id) { return (id & 0xFFFF0000u) == 0xFFFF0000u; }
static unsigned EnmScriptId(int stage, int half, int room, int n)
{
    return ((unsigned)(stage & 0x1F) << 24 | (unsigned)(room & 0xFF) << 16 | (unsigned)(n & 0xFFFF)) | (half ? 0x80000000u : 0u);
}
static int EnmCopyIdOk(int room, int n, int k) { return room > 0 && room < ENM_COPY_ROOMS && n >= 0 && n < ENM_COPY_NS && k >= 1 && k <= ENM_COPY_MAXK; }
static unsigned EnmCopyId(int room, int n, int k)
{
    return ENM_COPY_TAG << 24 | (unsigned)(room & 0x3F) << 10 | (unsigned)(n & 0xFF) << 2 | (unsigned)(k & 3);
}
static int EnmIsCopyId(unsigned id) { return (id >> 24) == ENM_COPY_TAG && (id >> 16 & 0xFF) == 0; }
// the session key of a copy (the dead ring): its id plus the stage and half it was made in
static unsigned EnmCopyKey(int stage, int half, unsigned copyId)
{
    return (unsigned)(stage & 0x1F) << 17 | (unsigned)(half & 1) << 16 | (copyId & 0xFFFF);
}

// ---------------------------------------------------------------- which types
// Copies (PLAN 3.1): 02 Whitestool / Toadstool, 03 Hummer, (06 Wheelchair until v3.37), 07 Gum Head, 08 Twin Victim / Bottom,
// 0A Sniffer Dog, 11 Patient ghost (script ids, no id override); the victims 0B (any variant), 0C v1 (baby), 0D v1 (old
// man) (FFFF00nn ids only). Never: 01, 04 mm, 05 Wall Man, 09 Tremer (the leech), 0C v0 Cynthia, 0D v0 Jasper, 0E Andrew,
// 0F Richard, 10 Walter / the Bodies, 12+.
static int EnmIsVictim(int type, int variant) { return type == 0x0B || ((type == 0x0C || type == 0x0D) && variant == 1); }
// v3.38 EN338: no 06 Wheelchair (the user's item 8: Wheelchairs stay vanilla; a Wheelchair's death sets story flag 0x530)
static int EnmCopyMonster(int type) { return type == 0x02 || type == 0x03 || type == 0x07 || type == 0x08 || type == 0x0A || type == 0x11; }
// idOver = the placement's id override (desc+0x2C), clearOver = the cleared descriptor's value ([0x10A0EAC])
static int EnmCopyEligible(int type, int variant, int mode, unsigned idOver, unsigned clearOver)
{
    if (mode != 0) return 0;  // event and boss modes 3 / 6 / 7 / 8
    if (EnmCopyMonster(type)) return idOver == clearOver;
    if (EnmIsVictim(type, variant)) return EnmIsGhostId(idOver);
    return 0;
}
// Follow (PLAN 4): monsters that walk (no mushrooms, no Tremers), and with "Monsters + Victims" the victims (ghost path)
static int EnmFollowMonster(int type) { return type == 0x03 || type == 0x07 || type == 0x08 || type == 0x0A || type == 0x11; }  // v3.38 EN338: no 06
// a standing enemy (work+0x3C): not down (9), HP 0 (0x0A), dead (0x0B), removed / scripted (>= 0x10)
static int EnmStanding(int cls) { return cls != 9 && cls != 0x0A && cls != 0x0B && cls < 0x10; }

// ---------------------------------------------------------------- bone budget (PLAN 2; the table's v5 rule, re_special B3)
// R = 400 + total / 10 records are kept free (the player, the companion, blends, fragmentation). Each enemy already in
// the room still needs 1.5 x its bones for its blends (H); a copy costs max(measured, 2.5 x its bones).
static const int kEnmBcost[32] = { 100, 100, 18, 150, 25, 58, 48, 185, 185, 20, 190, 98, 228, 90, 113, 90,
                                  145, 90, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
static int EnmBonesOk(int bones) { return bones > 0 && bones < 256; }
static int EnmTypeCost(int type) { return kEnmBcost[type >= 0 && type < 32 ? type : 31]; }
static int EnmCost(int type, int bones)
{
    const int t = EnmTypeCost(type), b = EnmBonesOk(bones) ? (bones * 5 + 1) / 2 : 0;
    return b > t ? b : t;
}
static int EnmHead(int type, int bones) { return EnmBonesOk(bones) ? (bones * 3 + 1) / 2 : EnmTypeCost(type) * 3 / 5; }
static int EnmReserve(int total) { return 400 + (total > 0 ? total : 0) / 10; }
static int EnmFits(int freeRec, int head, int cost, int total) { return freeRec - head - cost >= EnmReserve(total); }

// The bone pool size at a world reset (PLAN 2, hook 0x419830): BonePool 0 = stock, -1 = Auto (4096 while Copies or
// Follow is on), else 1408..16384 while one is on. The count that arrives (1408, or the table's Bone Pool Extender's) is
// never lowered.
#define ENM_POOL_AUTO (-1)
#define ENM_POOL_AUTO_N 4096
static int EnmPoolCount(int count, int setting, int featureOn)
{
    int want;
    if (!featureOn || setting == 0) return count;
    want = setting == ENM_POOL_AUTO ? ENM_POOL_AUTO_N : setting < 1408 ? 1408 : setting > 16384 ? 16384 : setting;
    return want > count ? want : count;
}

// ---------------------------------------------------------------- the copy plan (round robin, PLAN 3.2)
// Every original gets its first copy before any gets a second, so a tight budget spreads the copies. The simulation
// (tests) and the game loop share the order and the fit rule: EnmPlanNext gives the next (original, k) to try.
typedef struct { int i, k; } EnmStep;
static int EnmPlanNext(EnmStep *s, int n, int per)  // s starts at { -1, 1 }; returns 0 when done
{
    if (n <= 0 || per <= 0) return 0;
    if (++s->i >= n) {
        s->i = 0;
        if (++s->k > per) return 0;
    }
    return 1;
}
typedef struct { int type, bones; } EnmOrigCost;
// tests: how many copies of each original fit, with `freeRec` free records after the room's load and `head` the room's
// headroom; a made copy takes `bones` records at once (its skeleton) and adds 1.5 x bones to the headroom. Returns the
// copies made; made[i] = copies of original i.
static int EnmPlanSim(const EnmOrigCost *o, int n, int per, int freeRec, int head, int total, int listCount, int listMax,
                     int *made)
{
    EnmStep s = { -1, 1 };
    int count = 0, i;
    for (i = 0; i < n; ++i) made[i] = 0;
    while (EnmPlanNext(&s, n, per)) {
        const int cost = EnmCost(o[s.i].type, o[s.i].bones);
        if (listCount >= listMax) break;
        if (!EnmFits(freeRec, head, cost, total)) continue;
        freeRec -= EnmBonesOk(o[s.i].bones) ? o[s.i].bones : cost * 2 / 5;
        head += EnmHead(o[s.i].type, o[s.i].bones);
        ++made[s.i];
        ++count;
        ++listCount;
    }
    return count;
}

// ---------------------------------------------------------------- the dead ring (PLAN 3.3)
// A killed copy stays dead until the game is restarted, a world reset (New Game / load) or the world is left. Keys are
// EnmCopyKey values; 256 entries, the oldest is overwritten.
#define ENM_RING_N 256
typedef struct { unsigned key[ENM_RING_N]; int n; } EnmRing;
static void EnmRingClear(EnmRing *r) { r->n = 0; }
static int EnmRingHas(const EnmRing *r, unsigned key)
{
    int i, m = r->n < ENM_RING_N ? r->n : ENM_RING_N;
    for (i = 0; i < m; ++i)
        if (r->key[i] == key) return 1;
    return 0;
}
static void EnmRingAdd(EnmRing *r, unsigned key)
{
    if (EnmRingHas(r, key)) return;
    r->key[r->n % ENM_RING_N] = key;
    ++r->n;
    if (r->n >= 2 * ENM_RING_N) r->n -= ENM_RING_N;  // (keeps n % N and "full" without overflow)
}

// ---------------------------------------------------------------- Follow: pursuers, the door, the queue (PLAN 4.2)
#define ENM_FOLLOW_MAX 32  // v3.34 EN334: was 6 (a pack follows: FollowMax 6 by default, the ini 1..8); v3.38 EN338: 32 (1..32)
#define ENM_PURSUERS_N 256   // v3.38 EN338: the pursuers of a world visit (64 records + the copies' ledger, rounded up)
#define ENM_FOLLOW_DY 400.0f    // Eileen's vertical rule (0x449EC0)
typedef struct { float d2, dy; int ok; } EnmCand;  // horizontal distance squared, vertical distance, eligible + standing
// nearest first, within `range` horizontally and 400 vertically, at most `max` (<= ENM_FOLLOW_MAX); out = indexes
static int EnmPickPursuers(const EnmCand *c, int n, float range, int max, int *out)
{
    int used[64], m = 0, i, j;
    const float r2 = range * range;
    if (max > ENM_FOLLOW_MAX) max = ENM_FOLLOW_MAX;
    if (n > 64) n = 64;
    for (i = 0; i < n; ++i) used[i] = 0;
    while (m < max) {
        int best = -1;
        for (j = 0; j < n; ++j) {
            if (used[j] || !c[j].ok || !(c[j].d2 < r2) || !(c[j].dy < ENM_FOLLOW_DY && c[j].dy > -ENM_FOLLOW_DY)) continue;
            if (best < 0 || c[j].d2 < c[best].d2) best = j;
        }
        if (best < 0) break;
        used[best] = 1;
        out[m++] = best;
    }
    return m;
}
// a room change that carries followers: through a real door (door > 0; warps, holes, events and lifts use 0), in the same
// stage (2..8), not the Building elevator ride, the pursuer list taken in the room just left at most maxAgeMs before
// (prevRoom = the room the list was taken in)
static int EnmDoorCarries(int stage, int listStage, int listRoom, int room, int door, int elevator, unsigned ageMs,
                         unsigned maxAgeMs)
{
    return stage >= 2 && stage <= 8 && listStage == stage && listRoom > 0 && listRoom != room && door > 0 && !elevator &&
           ageMs <= maxAgeMs;
}
// the i-th follower (0 first) is due at delay + 1.5 s x i on the room timer
static float EnmFollowDue(int i, float delay) { return delay + 1.5f * (float)i; }
// what a due follower does now: 0 wait, 1 come to the door spot area (the player has left it, >= 300 away), 2 come to a
// ring spot away from the player (10 s waited), 3 stay behind (20 s waited: its record is untouched, it is home)
static int EnmArriveStep(float sinceDue, float playerFromDoor)
{
    if (sinceDue >= 20.0f) return 3;
    if (playerFromDoor >= 300.0f) return 1;
    if (sinceDue >= 10.0f) return 2;
    return 0;
}
// a wanderer: a record of a follow-eligible monster with a script id of this stage and half, recorded in this stage, in
// a room other than its home room (where its placement is). The game's own record move (the train event 0x4EED80: room
// 14 -> 22, 0x464420) is never one.
static int EnmIsWanderer(unsigned id, int type, int recStage, int recRoom, int stage, int half)
{
    const int home = EnmScriptRoom(id);
    if (!EnmIsScriptId(id) || EnmIsGhostId(id) || !EnmFollowMonster(type)) return 0;
    if (EnmScriptStage(id) != stage || EnmScriptHalf(id) != half || recStage != stage) return 0;
    if (recRoom <= 0 || recRoom == home) return 0;
    if (home == 14 && recRoom == 22) return 0;
    return 1;
}

// ---------------------------------------------------------------- spots
static unsigned EnmMix(unsigned h)  // murmur3 fmix32 (as the table's er_mix)
{
    h ^= h >> 16;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}
// candidate c (0..7) around a base: angle and distance from the id (the same spots on every visit)
static void EnmRingSpot(unsigned seed, int c, float rMin, float rMax, float *dx, float *dz)
{
    const unsigned h1 = EnmMix(seed ^ (0x9E3779B9u * (unsigned)(c + 1))), h2 = EnmMix(h1 ^ 0x55u);
    const float a = (float)(h1 >> 8) * (6.2831853f / 16777216.0f) + 0.7853982f * (float)c;  // spread over the circle
    const float d = rMin + (rMax - rMin) * (float)(h2 >> 8) / 16777216.0f;
    *dx = d * (float)sin(a);
    *dz = d * (float)cos(a);
}

// ================================================================ v3.30 EN330: copies made reliable, Follow fixed, Revive Enemies
// RE: staging/v330_feedback/w3_enemies/PLAN.md (decomp: room_4f07e0.c, en_463950.c, the type creates).

// ---------------------------------------------------------------- the bone pool at a room change
// RoomLoadSetMark 0x4F0A50 releases every bone (SgAnimeWorkBoneReset 0x4198B0) before the next room is built: nothing
// holds a bone across it, so the pool can be given a larger buffer right after it (SgAnimeWorkBoneSetBuffer 0x419840).
// The size Refined wants then (0 = keep the pool as it is).
static int EnmPoolGrowAt(int num, int setting, int featureOn)
{
    const int want = EnmPoolCount(num, setting, featureOn);
    return want > num ? want : 0;
}

// ---------------------------------------------------------------- Follow: who may follow, the door verdict
// A placement's spawn mode (mode word & 0xF): 0 ordinary. The Sniffer Dog's 3 (lies at a body: AI 0x11 at create), 6
// (waits: AI 0x15) and 7 (both) are ordinary dogs once awake; 6 / 7 (whole word) update only once story byte 0x13D80C6
// bit 0 is set (0x48EA70). The Hummer's 1 starts on the floor with 1 HP and gets up (0x4742F0). Every other mode is an
// event spawn (Gum Head 6 = its event model and no record, Wheelchair 6, Walter 6, ...) or a boss (Wall Man 8).
static int EnmFollowModeOk(int type, unsigned modeWord, int released, int aiState)
{
    const int m = (int)(modeWord & 0xF);
    if (m == 0) return 1;
    if (type == 0x0A && (m == 3 || m == 6 || m == 7)) {
        if ((modeWord == 6 || modeWord == 7) && !released) return 0;
        return aiState != 0x11;
    }
    return type == 0x03 && m == 1;
}
// Why a room change carries nobody (0 = it carries the pursuers)
enum { ENM_DOOR_OK, ENM_DOOR_STAGE, ENM_DOOR_OTHERSTAGE, ENM_DOOR_SAMEROOM, ENM_DOOR_WARP, ENM_DOOR_LIFT, ENM_DOOR_STALE,
       ENM_DOOR_FROM, ENM_DOOR_N };
static const char *const kEnmDoorWhy[ENM_DOOR_N] = {
    "a door", "not a monster world (stage outside 2..8)", "a change of world", "the same room again",
    "no door (a save load, a warp, a hole, an event or a cutscene move: door 0)", "a lift ride",
    "the list near you is too old (more than 6 s)", "the room change did not start in that room" };
// prevRoom = gGame.prevRoom (0xFD5A98, written by every room change request 0x4F7180 / 0x4F7220), -1 = not known
static int EnmDoorVerdict(int stage, int listStage, int listRoom, int room, int door, int elevator, unsigned ageMs,
                          unsigned maxAgeMs, int prevRoom)
{
    if (stage < 2 || stage > 8) return ENM_DOOR_STAGE;
    if (listStage != stage || listRoom <= 0) return ENM_DOOR_OTHERSTAGE;
    if (listRoom == room) return ENM_DOOR_SAMEROOM;
    if (door <= 0) return ENM_DOOR_WARP;
    if (elevator) return ENM_DOOR_LIFT;
    if (ageMs > maxAgeMs) return ENM_DOOR_STALE;
    if (prevRoom >= 0 && prevRoom != listRoom) return ENM_DOOR_FROM;
    return ENM_DOOR_OK;
}

// ---------------------------------------------------------------- kill bits (gEnemyManager.kill_bits, 0xFCF010, 32 dwords)
// EnemyKillBitStageBase 0x4639C0 (matched): bit = base(stage) + the placement's kill index (index <= 0: none).
static int EnmKillBitBase(int stage)
{
    switch (stage) {
    case 2: return 0x80;
    case 3: return 0x100;
    case 4: return 0x180;
    case 5: return 0x300;
    case 6: return 0x20;
    case 7: return 0x3C0;
    case 8: return 0x2C0;
    }
    return 0;
}
static int EnmKillBit(int stage, int index, int *word, unsigned *mask)  // 0 = no bit (stock: no bound; ours stays in 1024)
{
    const int bit = EnmKillBitBase(stage) + index;
    if (index <= 0 || bit < 0 || bit >= 32 * 32) return 0;
    *word = bit >> 5;
    *mask = 1u << (bit & 31);
    return 1;
}

// ---------------------------------------------------------------- Revive Enemies
// How the game keeps a killed enemy dead (en_463950.c, matched): (1) the destroy writes a record with state 0x0B (dead) and
// the next visit's gate (0x464F20 -> 4) restores the body; (2) the kill bit of its placement's kill index makes the gate
// answer 3 (never again) - set at the death by the Twin Victim (0x4C8DB0), the Tremer (1 death in 10, 0x48ACB0), the Wall
// Man's destroy and Walter's create, and by EnemyRecordAlloc 0x464060 when it reuses a dead record. Leaving a world through its hole
// (0x4651C0) deletes the records but keeps the bits.
// Revived: 02 Whitestool / Toadstool, 03 Hummer, 07 Gum Head, 08 Twin Victim / Bottom, 09 Tremer (no story flag in its
// code; the game itself brings most of them back), 0A Sniffer Dog. Never: 01, 04 mm, 05 Wall Man, 06 Wheelchair (its kill
// bit is a story mark: 0x4CAE30 with flag 0x530, 0x4CC430 with flag 0x51E), 0B-0F and 11 ghosts, 10 Walter / the Bodies.
static int EnmReviveType(int type) { return type == 0x02 || type == 0x03 || type == 0x07 || type == 0x08 || type == 0x09 || type == 0x0A; }
// a placement that revives: a revive type, spawn mode 0, its own script id (no id override)
static int EnmRevivePlacement(int type, int mode, unsigned idOver, unsigned clearOver)
{
    return mode == 0 && idOver == clearOver && EnmReviveType(type);
}
// a room whose script holds an event spawn (mode 6 / 7), Walter / the Bodies (type 10) or the Wall boss (05 mode 8) is a
// story room: nothing revives there. No stock event reads the enemy list or the kill bits (only the gate does), so no
// other fight gates the story.
static int EnmReviveRoomOk(int stage, int eventSpawns, int bosses) { return stage >= 2 && stage <= 8 && !eventSpawns && !bosses; }

// The dead ring without the copies of one room (killed copies come back when the room is entered again with Revive on).
// Returns how many were taken out.
static int EnmRingRemoveRoom(EnmRing *r, int stage, int half, int room)
{
    unsigned keep[ENM_RING_N];
    const int m = r->n < ENM_RING_N ? r->n : ENM_RING_N;
    int i, n = 0, out = 0;
    for (i = 0; i < m; ++i) {
        const unsigned k = r->key[i];
        if ((int)(k >> 17 & 0x1F) == (stage & 0x1F) && (int)(k >> 16 & 1) == (half & 1) && (int)(k >> 10 & 0x3F) == (room & 0x3F))
            ++out;
        else
            keep[n++] = k;
    }
    for (i = 0; i < n; ++i) r->key[i] = keep[i];
    r->n = n;
    return out;
}

// ---------------------------------------------------------------- spot tiers (copies and followers)
// Tried in order, 8 candidates each, until one passes: v3.29's ring first, then wider / nearer rings with smaller
// distances to the player and to other enemies (a copy next to an original that stands at the door, a crowded room).
typedef struct { float rMin, rMax, minPlayer, minEnemy; } EnmSpotTier;
#define ENM_COPY_TIERS 5
static const EnmSpotTier kEnmCopyTiers[ENM_COPY_TIERS] = {
    { 150.0f, 250.0f, 400.0f, 100.0f }, { 250.0f, 400.0f, 400.0f, 100.0f }, { 90.0f, 150.0f, 300.0f, 80.0f },
    { 400.0f, 600.0f, 300.0f, 80.0f },  { 150.0f, 300.0f, 200.0f, 60.0f } };
#define ENM_DOOR_TIERS 3
static const EnmSpotTier kEnmDoorTiers[2][ENM_DOOR_TIERS] = {  // step 1 (the player left the door), step 2 (10 s waited)
    { { 60.0f, 160.0f, 150.0f, 100.0f }, { 160.0f, 260.0f, 150.0f, 80.0f }, { 30.0f, 90.0f, 120.0f, 60.0f } },
    { { 150.0f, 300.0f, 150.0f, 100.0f }, { 300.0f, 450.0f, 150.0f, 80.0f }, { 60.0f, 150.0f, 120.0f, 60.0f } } };
// why a candidate failed (EnmSpotTest / the distance tests); the counts go to the log
enum { ENM_SPOT_OK, ENM_SPOT_WALL, ENM_SPOT_FLOOR, ENM_SPOT_DROP, ENM_SPOT_ROOM, ENM_SPOT_SIGHT, ENM_SPOT_PLAYER, ENM_SPOT_ENEMY,
       ENM_SPOT_N };

// ================================================================ v3.31 EN331: victim copies come back, the story ghosts are copied
// RE: staging/v331_feedback/w2_enemies/PLAN.md (decomp: en_4ad140.c, ghost_484730.c, en_463950.c, pl_542c80.c).

// ---------------------------------------------------------------- which ghosts
// EnemyTypeIsGhost 0x463910: 0x0B..0x0F (the Patient 0x11 is not one: it dies, its class 0x0B is death)
static int EnmGhostType(int type) { return type >= 0x0B && type <= 0x0F; }
// the four story ghosts: Cynthia's ghost 0C v0, Jasper 0D v0, Andrew 0E, Richard 0F (0C v1 / 0D v1 are victims)
static int EnmIsStoryGhost(int type, int variant)
{
    return ((type == 0x0C || type == 0x0D) && variant == 0) || type == 0x0E || type == 0x0F;
}
// EnmCopyEligible plus the story ghosts (when `story` is on): spawn mode 0 and their own FFFF00nn ghost id
static int EnmCopyEligible2(int type, int variant, int mode, unsigned idOver, unsigned clearOver, int story)
{
    if (EnmCopyEligible(type, variant, mode, idOver, clearOver)) return 1;
    return story && mode == 0 && EnmIsStoryGhost(type, variant) && EnmIsGhostId(idOver);
}
// where a story ghost is never copied (0 = it may be): the two story writes a copy could make
//  1 = Andrew in stage 4 before story flag 0x2DC: the end of a Sword pin of any type-0x0E enemy runs 0x4CE050, which
//      there plays the "Andrew pinned" message and sets 0x2DC (a pinned copy would do it for him);
//  2 = Richard in stage 5 room 3: ScratchThink 0x4BFCB0 counts 30 s there (while 0x3AE is clear) and then sets flag
//      0x3BF and leaves (writing a follower record mid-room).
enum { ENM_STORY_OK, ENM_STORY_ANDREW, ENM_STORY_RICHARD };
static int EnmStoryCopyRoom(int type, int stage, int room, int flag2DC)
{
    if (type == 0x0E && stage == 4 && !flag2DC) return ENM_STORY_ANDREW;
    if (type == 0x0F && stage == 5 && room == 3) return ENM_STORY_RICHARD;
    return ENM_STORY_OK;
}
// an original that its copies may stand beside: a monster standing (EnmStanding); a ghost in any class below the removed
// / scripted ones (>= 0x10): down (9 / 0x0A) and pinned (0x0B) included, as ghosts never die (v3.30 made no copies while
// a victim lay down: they were lost until it stood at a later visit). (0x0F stays in, as in v3.30: the restore
// 0x4AD330 gives a victim its record's state, 0x0F after a follow destroy, until its AI sets another.)
static int EnmCopyBeside(int type, int cls)
{
    if (EnmGhostType(type)) return cls >= 0 && cls < 0x10;
    return EnmStanding(cls);
}
// the dead ring takes killed copies only: a ghost's class 0x0B is the Sword's pin, never death
static int EnmCopyKilled(int type, int cls) { return cls == 0x0B && !EnmGhostType(type); }

// ---------------------------------------------------------------- a ghost record away from the room
// EnemyGhostRegenHp 0x4853F0 runs on every ghost record every frame (0x4647F0 -> 0x463890 -> 0x486150): the countdown
// (+0x3C) falls by the frame time to 0, then the HP (+0x28) grows by 2^difficulty per second up to the max (0x468E00).
// The same for `elapsed` seconds at once (the closed form; frame-rate independent).
static void EnmGhostDecay(float *countdown, float *hp, float hpMax, int difficulty, float elapsed)
{
    float rest = elapsed > 0.0f ? elapsed : 0.0f, rate;
    if (*countdown > 0.0f) {
        if (*countdown > rest) {
            *countdown -= rest;
            return;
        }
        rest -= *countdown;
        *countdown = 0.0f;
    }
    if (difficulty < 0) difficulty = 0;
    if (difficulty > 4) difficulty = 4;
    rate = (float)(1 << difficulty);
    if (*hp < hpMax) {
        const float v = *hp + rate * rest;
        *hp = v < hpMax ? v : hpMax;
    }
}
// the clock of those decays: the enemy room timer (0xFD0BA0) grows by the enemy frame time exactly when the records
// age (EnemyManagerTimerUpdate 0x4647F0: mode 1, no world exit); a room change restarts it (at 0, or 2 / 3 in some
// stage 7 rooms): that step counts 0. One step is capped at 1 s (a stall or a junk read).
static float EnmClockStep(float prev, float now)
{
    const float d = now - prev;
    if (!(d > 0.0f)) return 0.0f;
    return d < 1.0f ? d : 1.0f;
}

// ---------------------------------------------------------------- the ghost copies' own records (Refined's store)
// A ghost copy's destroy writes its record at room exit (state 0x0F follow / 0x0B pinned, countdown, HP, place); the
// copy-record purge takes it out of the game's table (copies are never saved) into this store, keyed by EnmCopyKey.
// The next visit of that room puts it back for the copy's own create (the gate's 4 = the type's restore: down with its
// countdown, pinned, or standing), then deletes it again.
// v3.31 EN331-002 (inspection 3): 1024 (was 96: 32 originals at x4, while snapshots build up over a world's rooms):
// 341 ghost originals at x4 per world visit, far above any world's ghost placements; the store is cleared at each
// world reset; an eviction is still reported by EnmSnapPut (logged by the caller)
#define ENM_SNAP_N 1024
#define ENM_REC_SIZE 0x50
typedef struct { unsigned key; int room, used; float clock; unsigned age; unsigned char rec[ENM_REC_SIZE]; } EnmSnap;
typedef struct { EnmSnap s[ENM_SNAP_N]; unsigned age; } EnmSnapStore;
static void EnmSnapClear(EnmSnapStore *st)
{
    int i;
    for (i = 0; i < ENM_SNAP_N; ++i) st->s[i].used = 0;
    st->age = 0;
}
static EnmSnap *EnmSnapFind(EnmSnapStore *st, unsigned key)
{
    int i;
    for (i = 0; i < ENM_SNAP_N; ++i)
        if (st->s[i].used && st->s[i].key == key) return &st->s[i];
    return 0;
}
// the same key is replaced; else a free entry; else the oldest goes. Returns 1 when another copy's snapshot was evicted
static int EnmSnapPut(EnmSnapStore *st, unsigned key, int room, float clock, const unsigned char *rec)
{
    EnmSnap *e = EnmSnapFind(st, key);
    int i, b, evicted = 0;
    if (!e) {
        for (i = 0, b = -1; i < ENM_SNAP_N; ++i) {
            if (!st->s[i].used) {
                b = i;
                break;
            }
            if (b < 0 || st->s[i].age < st->s[b].age) b = i;
        }
        e = &st->s[b];
        evicted = e->used != 0;
    }
    e->key = key, e->room = room, e->clock = clock, e->age = ++st->age, e->used = 1;
    for (i = 0; i < ENM_REC_SIZE; ++i) e->rec[i] = rec[i];
    return evicted;
}
static int EnmSnapCount(const EnmSnapStore *st)
{
    int i, n = 0;
    for (i = 0; i < ENM_SNAP_N; ++i) n += st->s[i].used != 0;
    return n;
}

// ---------------------------------------------------------------- copies of flying monsters (the Hummer, 03)
// A Hummer flies (Building World room 5:4: placements 720 above the floor): v3.30's spot test wanted a floor at most
// 400 below the candidate (60 % of the candidates failed "floor too low"). A flyer copy keeps the original's height
// above its own floor instead: the floor under the candidate must lie within 400 of the original's floor, and the copy
// is put that high above it. y grows downwards (the floor y is larger than the flyer's y).
#define ENM_FLYER_MIN 300.0f   // higher than this above its floor = a flyer
static int EnmIsFlyer(int type, float baseY, float floor0Y) { return type == 0x03 && floor0Y - baseY > ENM_FLYER_MIN; }
static int EnmFlyerFloorOk(float floor0Y, float floorY) { return floorY - floor0Y <= 400.0f && floor0Y - floorY <= 400.0f; }
static float EnmFlyerY(float baseY, float floor0Y, float floorY) { return floorY - (floor0Y - baseY); }

// ================================================================ v3.33 EN333: event dogs copied, Follow carries its files
// RE: staging/v333_feedback/w6_enemies/PLAN.md (decomp: en_48bdb0.c SnifferDogCreate 0x48E690 / SnifferDogTask 0x492C00,
// stagefile_4f3460.c, file.c FileRequestAsync 0x573470, res_417840.c 0x417D40, en_466c30.c).

// ---------------------------------------------------------------- the event dogs (Subway World room 2:2 and others)
// A Sniffer Dog placement with spawn mode 3 (lies at a body: AI 0x11 at create), 6 (waits: AI 0x15) or 7 (both) is an
// ordinary dog whose task does nothing (no update, no draw) while its whole mode word is 6 / 7 and story flag 0x130
// (byte 0x13D80C6 bit 0) is clear (0x492C00). The flag is global and no event reads the dog list (only the first dog's
// D_00FD0F54 countdown, 0x492C1F) or the dogs' kill bits, so a copy that keeps the original's mode word sleeps, wakes and
// fights exactly as the original: v3.32 left these dogs out ("event spawns", the user's QA 2026-11-20).
static int EnmEventDog(int type, unsigned modeWord)
{
    const int m = (int)(modeWord & 0xF);
    return type == 0x0A && (m == 3 || m == 6 || m == 7);
}
// EnmCopyEligible2 plus the event dogs (when `dogs` is on): their own script id (no id override)
static int EnmCopyEligible3(int type, int variant, unsigned modeWord, unsigned idOver, unsigned clearOver, int story, int dogs)
{
    if (EnmCopyEligible2(type, variant, (int)(modeWord & 0xF), idOver, clearOver, story)) return 1;
    return dogs && EnmEventDog(type, modeWord) && idOver == clearOver;
}
// the mode word of a copy's descriptor: spawn mode 0 with the original's variant (v3.29), but an event dog keeps its
// whole word (its variant and its 3 / 6 / 7: the copy sleeps and wakes with it)
static unsigned EnmCopyModeWord(int type, unsigned modeWord, int dogs)
{
    if (dogs && EnmEventDog(type, modeWord)) return modeWord & 0xF0Fu;
    return modeWord & 0xF00u;
}
// an original standing in the room may get copies in its spawn mode: 0, or an event dog's 3 / 6 / 7
static int EnmCopyModeOk(int type, unsigned modeWord, int dogs)
{
    return (modeWord & 0xF) == 0 || (dogs && EnmEventDog(type, modeWord));
}

// ---------------------------------------------------------------- Follow: who is near, the list kept for the door
// A flying Hummer hangs up to 720 above its floor (Building World 5:17, the user's log: "5 above / below you"): it is
// noted up to 1100 above or below her; every other monster within Eileen's 400 (0x449EC0).
#define ENM_FOLLOW_DY_FLYER 1100.0f
static float EnmFollowDyMax(int type) { return type == 0x03 ? ENM_FOLLOW_DY_FLYER : ENM_FOLLOW_DY; }
// v3.33: the candidate's own vertical limit (dyMax <= 0: ENM_FOLLOW_DY, as v3.29 - v3.32)
static int EnmCandDyOk(float dy, float dyMax)
{
    const float m = dyMax > 0.0f ? dyMax : ENM_FOLLOW_DY;
    return dy < m && dy > -m;
}
typedef struct { float d2, dy; int ok; float dyMax; } EnmCand2;
// EnmPickPursuers with a per-candidate vertical limit
static int EnmPickPursuers2(const EnmCand2 *c, int n, float range, int max, int *out)
{
    int used[64], m = 0, i, j;
    const float r2 = range * range;
    if (max > ENM_FOLLOW_MAX) max = ENM_FOLLOW_MAX;
    if (n > 64) n = 64;
    for (i = 0; i < n; ++i) used[i] = 0;
    while (m < max) {
        int best = -1;
        for (j = 0; j < n; ++j) {
            if (used[j] || !c[j].ok || !(c[j].d2 < r2) || !EnmCandDyOk(c[j].dy, c[j].dyMax)) continue;
            if (best < 0 || c[j].d2 < c[best].d2) best = j;
        }
        if (best < 0) break;
        used[best] = 1;
        out[m++] = best;
    }
    return m;
}
// The enemy list is empty during the room's exit teardown (0x462A20 clears it after the destroys wrote the records):
// the list taken before is kept for the door (1 = keep it, do not refresh). v3.32 kept it in any empty room too, so a
// room without enemies aged it past 6 s and the door said "the list is too old" (the user's log, every empty room).
// An empty list is kept only while it holds someone; an empty room refreshes its (empty) list and its time.
static int EnmPurKeep(int listEmpty, int sameRoom, int kept) { return listEmpty && sameRoom && kept > 0; }

// ---------------------------------------------------------------- Follow: the follower's files
// A monster can only be made where its files are resident: v3.32's followers stayed home whenever the next room's
// file list lacks their type (the user's log: "its files are not loaded in room 3"; most rooms list only their own
// monsters). v3.33 loads them as the room loader does (FileRequestAsync 0x573470 for every whole file, the queue run to
// the end 0x573370(0), the set-up 0x417D40 that RoomFilesSetup does), at the room load before the room's script, in the
// arena above the room mark: the next room change parks them with the room's own files (HeapMarkRoomLoad 0x4EF5F0)
// after the follower was destroyed with the room.
// A file's whole key: 0xF000Fkkk (bit 27 = the second table) of a container entry key 0xF0NNFkkk; 0 = not a file key
static unsigned EnmWholeKey(unsigned h) { return (h & 0xF000F000u) == 0xF000F000u ? h & 0xF800FFFFu : 0u; }
// adds the whole key of h to keys[0..n) once; returns the new count
static int EnmKeyAdd(unsigned *keys, int n, int max, unsigned h)
{
    const unsigned k = EnmWholeKey(h);
    int i;
    if (!k) return n;
    for (i = 0; i < n; ++i)
        if (keys[i] == k) return n;
    if (n < max) keys[n++] = k;
    return n;
}
// The arena rule: the files fit with `keep` bytes left for what the room may still load (the neighbour preload task
// 0x4F36B0 and this room's event lists). The static budget of the Playable Eileen s27 work (re/arena_budget.tsv, 48
// contexts) bounds that at 29.2 MB after a room list (Building 2nd room 10, HD Eileen included): keep = 30 MB.
#define ENM_ARENA_KEEP (30u << 20)
static unsigned EnmFileBlock(int size) { return size > 0 ? ((unsigned)size + 0x7FFu) & ~0x7FFu : 0u; }
static int EnmArenaFits(unsigned freeBytes, unsigned need, unsigned keep) { return freeBytes >= need && freeBytes - need >= keep; }

// ================================================================ v3.34 EN334: released event dogs revive, copies follow
// RE: staging/v334_feedback/w1_enemies/PLAN.md (decomp: en_48bdb0.c SnifferDogCreate 0x48E690 / SnifferDogRestoreRecord
// 0x48DB70, en_463950.c EnemySpawnGate 0x464F20 / EnemyRecordRestore 0x464470; the Subway room 2:2 script and its events
// 00A / 04A / 04C in the twin's research/event_scripts/out/stage02_1st.txt).

// ---------------------------------------------------------------- Revive Enemies: the released event dogs
// The event dogs of Subway room 2:2 (02020001 mode 7, 02020002 / 02020003 mode 6, kill indexes 2 / 3 / 4) are its only
// "event spawns". Their event is over once story flag 0x130 ("Creatures I", byte 0x13D80C6 bit 0) is set: the room's two
// events need it clear (00A, the cutscene that sets it; 04A "Washrooms"), its examine point 04C needs it set and reads no
// enemy; no event reads the dogs, their kill bits or the dog list. So a room whose event spawns are all released event
// dogs (and no boss) is no story room for Revive Enemies; the dogs are replayed awake (spawn mode cleared: mode 6 would
// wait, mode 7 / 3 lie as a body (AI 0x11 sets class 0x0B), as Follow already clears it).
static int EnmReviveEventDog(int type, unsigned modeWord, unsigned idOver, unsigned clearOver, int released)
{
    return released && EnmEventDog(type, modeWord) && idOver == clearOver;
}
// eventSpawns = the room's mode 6 / 7 placements, releasedDogs = how many of them are released event dogs
static int EnmReviveRoomOk2(int stage, int eventSpawns, int releasedDogs, int bosses)
{
    const int left = eventSpawns - releasedDogs;
    return EnmReviveRoomOk(stage, left > 0 ? left : 0, bosses);
}
// a copy's mode word: v3.33's (EnmCopyModeWord), but a released event dog's copy is made awake (mode 0): v3.33 made a
// mode 7 copy after the release, and its AI 0x11 laid it down as a body at once (class 0x0B: the dead ring took it as killed)
static unsigned EnmCopyModeWord2(int type, unsigned modeWord, int dogs, int released)
{
    if (released && EnmEventDog(type, modeWord)) return modeWord & 0xF00u;
    return EnmCopyModeWord(type, modeWord, dogs);
}

// ---------------------------------------------------------------- Follow: copies follow as monsters do
// A copy id (0x1E000000 | room << 10 | n << 2 | k) holds its original's room and object number: its descriptor can be
// rebuilt from that room's script anywhere in the stage.
static int EnmCopyRoomOf(unsigned cid) { return (int)(cid >> 10 & 0x3F); }
static int EnmCopyNOf(unsigned cid) { return (int)(cid >> 2 & 0xFF); }
static int EnmCopyKOf(unsigned cid) { return (int)(cid & 3); }
// Who may pursue (0 = yes, else why not): a victim with its FFFF00nn id (ghost) by the victims' rule; a monster of a
// follow type in an awake mode (EnmFollowModeOk) with its script id and a kill index; a copy (copy id) as a monster of
// its type, without the script id and the kill index (its own room is kept free of it by the copies' ledger). Copies of
// victims and ghosts never (not a follow monster type).
enum { ENM_PUR_YES, ENM_PUR_OTHER, ENM_PUR_EVENT, ENM_PUR_NOKILL };
static int EnmPursuerWhy(int copy, int ghost, int victims, int type, unsigned modeWord, int released, int aiState, int scriptId,
                         int killIdx)
{
    if (ghost) return !victims ? ENM_PUR_OTHER : (modeWord & 0xF) != 0 ? ENM_PUR_EVENT : ENM_PUR_YES;
    if (!EnmFollowMonster(type) || (!copy && !scriptId)) return ENM_PUR_OTHER;
    if (!EnmFollowModeOk(type, modeWord, released, aiState)) return ENM_PUR_EVENT;
    if (!copy && killIdx <= 0) return ENM_PUR_NOKILL;
    return ENM_PUR_YES;
}
// FollowMax: the default 6 (a pack); the ini takes 1..ENM_FOLLOW_MAX (8, enemies_sim.h above)
#define ENM_FOLLOW_DEFAULT ENM_FOLLOW_MAX  // v3.38 EN338: was 6 (every follower of a door up to the arrays; space decides)
static int EnmFollowMaxIni(int v) { return v < 1 ? 1 : v > ENM_FOLLOW_MAX ? ENM_FOLLOW_MAX : v; }

// ---------------------------------------------------------------- the copies' ledger (copies that followed you)
// A copy that came through a door lives in that room for the rest of the world visit: its key (EnmCopyKey), its room and
// its last record (HP, state, place; written by its destroy at each room exit). Never saved; cleared at a world reset
// (New Game / title load), a save load, leaving the world, Enemy Copies Off, Enemies Follow Off (it goes home). While it
// is in the ledger its own room does not make it; when it dies its entry goes (the dead ring keeps it dead).
#define ENM_CW_N 128  // v3.38 EN338: was 48 (copies follow for the whole world visit)
typedef struct { unsigned key, cid; int room, type, variant, used; unsigned age; unsigned char rec[ENM_REC_SIZE]; float clock; } EnmCw;  // v3.35 GH335: + clock (a ghost copy's record ages)
typedef struct { EnmCw e[ENM_CW_N]; unsigned age; } EnmCwLedger;
static void EnmCwClear(EnmCwLedger *l)
{
    int i;
    for (i = 0; i < ENM_CW_N; ++i) l->e[i].used = 0;
    l->age = 0;
}
static EnmCw *EnmCwFind(EnmCwLedger *l, unsigned key)
{
    int i;
    for (i = 0; i < ENM_CW_N; ++i)
        if (l->e[i].used && l->e[i].key == key) return &l->e[i];
    return 0;
}
static int EnmCwCount(const EnmCwLedger *l)
{
    int i, n = 0;
    for (i = 0; i < ENM_CW_N; ++i) n += l->e[i].used != 0;
    return n;
}
// the same key is updated; else a free entry; else the oldest goes. Returns 1 when another copy's entry was evicted
static int EnmCwPut(EnmCwLedger *l, unsigned key, unsigned cid, int room, int type, int variant, const unsigned char *rec)
{
    EnmCw *e = EnmCwFind(l, key);
    int i, b, evicted = 0;
    if (!e) {
        for (i = 0, b = -1; i < ENM_CW_N; ++i) {
            if (!l->e[i].used) {
                b = i;
                break;
            }
            if (b < 0 || l->e[i].age < l->e[b].age) b = i;
        }
        e = &l->e[b];
        evicted = e->used != 0;
    }
    e->key = key, e->cid = cid, e->room = room, e->type = type, e->variant = variant, e->age = ++l->age, e->used = 1;
    for (i = 0; i < ENM_REC_SIZE; ++i) e->rec[i] = rec[i];
    return evicted;
}
// a record of a copy (its destroy at a room exit): -1 not in the ledger, 0 kept (its room and record follow it), 1 it
// died there (state 0x0B): the entry goes
static int EnmCwOnRecord(EnmCwLedger *l, unsigned key, const unsigned char *rec)
{
    EnmCw *e = EnmCwFind(l, key);
    short state;
    int i;
    if (!e) return -1;
    state = (short)(rec[0x26] | rec[0x27] << 8);
    if (state == 0x0B) {
        e->used = 0;
        return 1;
    }
    e->room = (signed char)rec[0x37];
    for (i = 0; i < ENM_REC_SIZE; ++i) e->rec[i] = rec[i];
    e->age = ++l->age;
    return 0;
}

// ---------------------------------------------------------------- wanderers: where one is put, whether it is drawn
// EnmWanderSpot (enemies.inc): its record's spot when the copies' floor test passes there (tested from 100 above it, the
// spot snapped onto that floor), else the first ring spot around it that passes, else the record's spot as v3.33
enum { ENM_WSPOT_RECORD, ENM_WSPOT_FLOOR, ENM_WSPOT_RING, ENM_WSPOT_KEPT, ENM_WSPOT_FLYER, ENM_WSPOT_GHOST, ENM_WSPOT_N };  // v3.35 GH335: + GHOST
static const char *const kEnmWspot[ENM_WSPOT_N] = { "its record's spot", "its record's spot, put onto the floor",
                                                    "a spot near its record's (no floor or room to stand there)",
                                                    "its record's spot (no tested spot near it)", "its record's spot (a flyer)",
                                                    "its record's spot (a ghost: it passes walls)" };  // v3.35 GH335
// the floor found within 5 units of the record's height keeps the record's spot
static int EnmSnapKind(float recY, float floorY) { return fabsf(floorY - recY) < 5.0f ? ENM_WSPOT_RECORD : ENM_WSPOT_FLOOR; }
// a class the game draws (EnemyIsActive 0x45CA40: 1..0x0C or 0x12)
static int EnmClassDrawn(int cls) { return cls > 0 && (cls <= 0x0C || cls == 0x12); }

// ================================================================ v3.35 GH335: victims pursue, late copies, ghost copies follow, the down countdown
// RE: staging/v335_feedback/w1_ghosts/PLAN.md (decomp: ghost_484730.c EnemyGhostArrivalCheck 0x486150 /
// EnemyGhostFollowerCreate 0x486070 / EnemyGhostRealFromPlaceholder 0x4852B0, en_463950.c EnemyGhostMayEnterRoom
// 0x464880 / EnemySpawnGate 0x464F20, en_47dc40.c the ghost hit 0x47F6xx and the down step 0x481870, ghost_4823e0.c
// EnemyGhostCountdownSet 0x483F20 / GhostUpdate 0x484430).

// ---------------------------------------------------------------- Follow: victims pursue you
// v3.34 took a victim only while it stood within FollowRange (800) at the door, and the tag list was cleared at every
// door: a victim that followed once never followed again (the user's log 16:48:02 / 16:48:26). A victim that follows
// now PURSUES: it is tagged again at every later door (in the room, far away, or not arrived yet) until it is downed
// (HP 0) or pinned, you leave the world, load, or it fails to catch up for ENM_PURSUE_MISSES doors in a row.
#define ENM_VICTIM_RANGE 1500.0f  // [Gameplay] FollowRangeVictims (ini only, 200..3000): a victim is taken this far away
#define ENM_PURSUE_MISSES 3       // [Gameplay] FollowKeepDoors (ini only, 0 = v3.34: no pursuit, 1..8)
static int EnmKeepDoorsIni(int v) { return v < 0 ? 0 : v > ENM_FOLLOW_MAX ? ENM_FOLLOW_MAX : v; }
static float EnmVictimRangeIni(double f) { return (float)(f < 200.0 ? 200.0 : f > 3000.0 ? 3000.0 : f); }
// a candidate with its own range (squared) and priority (a pursuing victim: taken first, wherever it is in the room)
typedef struct { float d2, dy, dyMax, r2; int ok, prio; } EnmCand3;
static int EnmPickPursuers3(const EnmCand3 *c, int n, int max, int *out)
{
    int used[64], m = 0, i, j, pass;
    if (max > ENM_FOLLOW_MAX) max = ENM_FOLLOW_MAX;
    if (n > 64) n = 64;
    for (i = 0; i < n; ++i) used[i] = 0;
    for (pass = 0; pass < 2; ++pass)
        while (m < max) {
            int best = -1;
            for (j = 0; j < n; ++j) {
                if (used[j] || !c[j].ok || (pass == 0) != (c[j].prio != 0)) continue;
                if (pass == 1 && (!(c[j].d2 < c[j].r2) || !EnmCandDyOk(c[j].dy, c[j].dyMax))) continue;
                if (best < 0 || c[j].d2 < c[best].d2) best = j;
            }
            if (best < 0) break;
            used[best] = 1;
            out[m++] = best;
        }
    return m;
}
// the pursuing victims of this world visit (never saved)
// v3.38 EN338: every kind pursues (monsters, copies, victims, story ghosts): + copy, ghost (a ghost type: the game's arrival
// brings an original, Refined's queue a copy), door (the room visit whose door handled it); ENM_PURSUERS_N entries (was 8)
typedef struct { unsigned id; int type, variant, stage, misses, seen, standing, used, copy, ghost, door; } EnmPursuer;
typedef struct { EnmPursuer e[ENM_PURSUERS_N]; } EnmPursuers;
static void EnmPursueClear(EnmPursuers *p)
{
    int i;
    for (i = 0; i < ENM_PURSUERS_N; ++i) p->e[i].used = 0;
}
static EnmPursuer *EnmPursueFind(EnmPursuers *p, unsigned id)
{
    int i;
    for (i = 0; i < ENM_PURSUERS_N; ++i)
        if (p->e[i].used && p->e[i].id == id) return &p->e[i];
    return 0;
}
static int EnmPursueCount(const EnmPursuers *p)
{
    int i, n = 0;
    for (i = 0; i < ENM_PURSUERS_N; ++i) n += p->e[i].used != 0;
    return n;
}
// v3.38 EN338: one that followed (or waits for a later door): 1 new, 0 refreshed (its misses start again), -1 full
static int EnmPursueAdd2(EnmPursuers *p, unsigned id, int type, int variant, int stage, int serial, int copy, int ghost)
{
    EnmPursuer *e = EnmPursueFind(p, id);
    int i, r = 0;
    if (!e) {
        for (i = 0; i < ENM_PURSUERS_N && p->e[i].used; ++i) {}
        if (i >= ENM_PURSUERS_N) return -1;
        e = &p->e[i];
        e->door = 0;
        r = 1;
    }
    e->id = id, e->type = type, e->variant = variant, e->stage = stage, e->misses = 0, e->seen = serial, e->standing = 1;
    e->copy = copy, e->ghost = ghost;
    e->used = 1;
    return r;
}
// a victim that followed through a door (seen = the room visit it was seen in: `serial`): 1 new, 0 refreshed, -1 full
static int EnmPursueAdd(EnmPursuers *p, unsigned id, int type, int variant, int stage, int serial)
{
    return EnmPursueAdd2(p, id, type, variant, stage, serial, 0, 1);
}
// pursuers not seen in the room visit `serial`: they hold their FollowMax slots while they catch up
static int EnmPursueAbsent(const EnmPursuers *p, int serial)
{
    int i, n = 0;
    for (i = 0; i < ENM_PURSUERS_N; ++i) n += p->e[i].used && p->e[i].seen != serial;
    return n;
}
// v3.38 EN338: + DIED (a monster or a copy killed), DENIED (the game's story refusal 0x464880), HOME (a copy that left the
// copies' ledger: made in its own room again), OFF (Enemies Follow takes no ghosts now)
enum { ENM_PURSUE_KEEP, ENM_PURSUE_DOWN, ENM_PURSUE_PINNED, ENM_PURSUE_GONE, ENM_PURSUE_BEHIND, ENM_PURSUE_MAX, ENM_PURSUE_DIED,
       ENM_PURSUE_DENIED, ENM_PURSUE_HOME, ENM_PURSUE_N };
static const char *const kEnmPursueWhy[ENM_PURSUE_N] = {
    "it keeps pursuing you", "downed (HP 0)", "pinned by the Sword", "gone from the world (no record, or its record left it)",
    "it did not catch up", "FollowMax is full", "killed", "a story flag keeps it out (the game's room rule)",
    "it is made in its own room again (no longer in the copies' ledger)" };
// a pursuer seen in the room (class, HP): downed (9 / 0x0A at HP 0) or pinned (0x0B) ends it; else it is seen here
static int EnmPursueOnSight(EnmPursuer *e, int serial, int cls, float hp)
{
    if (cls == 0x0B) return ENM_PURSUE_PINNED;
    if ((cls == 9 || cls == 0x0A) && !(hp > 0.0f)) return ENM_PURSUE_DOWN;
    e->seen = serial;
    e->standing = EnmStanding(cls);
    return ENM_PURSUE_KEEP;
}
// at a door, a pursuer that was not seen in the room you left: its record (recState -1 = none) says whether it is still on
// its way. recRoom 0x63 = the game moved it out of the world (EnemyLeaveWorldHole 0x4651C0). *misses counts the doors.
static int EnmPursueAtDoor(int *misses, int maxMisses, int stage, int recState, int recStage, int recRoom, float recCountdown)
{
    if (recState <= 0 || recStage != stage || recRoom == 0x63 || recRoom <= 0) return ENM_PURSUE_GONE;
    if (recState == 0x0B) return ENM_PURSUE_PINNED;
    if (recCountdown > 0.0f && (recState == 9 || recState == 0x0A)) return ENM_PURSUE_DOWN;
    if (++*misses >= maxMisses) return ENM_PURSUE_BEHIND;
    return ENM_PURSUE_KEEP;
}

// ---------------------------------------------------------------- Follow: a victim that followed before, victims' copies
// The root of "they follow only for one room" (the user's log 16:48:26): a victim that came in through the game's arrival
// is a follower placeholder first (EnemyGhostFollowerCreate 0x486070, mode word variant << 8), and the real ghost made from
// it (EnemyGhostRealFromPlaceholder 0x4852B0) gets the mode word variant << 8 | 4: v3.34 read that 4 as an event spawn
// (ENM_PUR_EVENT, "event spawns not awake"), so a victim that had followed (or roamed in) once never followed again.
// Mode 0 (its placement) and 4 (arrived) are a victim's ordinary modes.
static int EnmVictimModeOk(unsigned modeWord) { return (modeWord & 0xF) == 0 || (modeWord & 0xF) == 4; }
// v3.35.1 HF3351: an enemy's HP from its work's HP block pointer (work +0x34, EnemyHpGet 0x44FD30 = *[work+0x34]).
// A victim the game has just re-made (it followed you or roamed in) has no HP block yet: v3.35.0 called 0x44FD30
// on it and faulted at 0x44FD3A (fld [NULL]), which turned Enemy Copies / Follow off for the session. No block (or a
// pointer below 0x10000) = the fallback.
static float EnmHpFromBlock(const float *blk, float fallback)
{
    return (unsigned long)(size_t)blk >= 0x10000ul ? blk[0] : fallback;
}
// v3.34: copies of victims never followed (not a follow monster). Now a victim's copy follows on its own as the
// monsters' copies do (FollowCopies, Monsters + Victims), through the copy path (its record, the queue, the copies'
// ledger); the story ghosts' copies never (as their originals).
static int EnmPursuerWhy2(int copy, int ghost, int victims, int type, int variant, unsigned modeWord, int released,
                          int aiState, int scriptId, int killIdx)
{
    if (ghost && !copy) return !victims ? ENM_PUR_OTHER : EnmVictimModeOk(modeWord) ? ENM_PUR_YES : ENM_PUR_EVENT;
    if (copy && EnmIsVictim(type, variant)) return !victims ? ENM_PUR_OTHER : (modeWord & 0xF) != 0 ? ENM_PUR_EVENT : ENM_PUR_YES;
    return EnmPursuerWhy(copy, ghost, victims, type, modeWord, released, aiState, scriptId, killIdx);
}
// a ghost copy's ledger record at a room exit: as EnmCwOnRecord, but its 0x0B is the Sword's pin, never death (kept)
static int EnmCwOnRecord2(EnmCwLedger *l, unsigned key, const unsigned char *rec, int ghost)
{
    EnmCw *e;
    int i;
    if (!ghost) return EnmCwOnRecord(l, key, rec);
    if ((e = EnmCwFind(l, key)) == 0) return -1;
    e->room = (signed char)rec[0x37];
    for (i = 0; i < ENM_REC_SIZE; ++i) e->rec[i] = rec[i];
    e->age = ++l->age;
    return 0;
}

// ---------------------------------------------------------------- Enemy Copies: victims that come in later
// v3.34 made copies only at the room load, for the originals present then ("room 2:9 copies x4: ... none in the room"):
// a victim that drifts in later through the game's arrival (0x486150) or follows you came alone. Every room visit
// keeps the set of victim originals already handled (present at the load, or handled since); a new one gets its copies
// beside it once it is the real ghost (class 1..0x0E: not the follower placeholder 0x0F, nothing removed / scripted).
#define ENM_LATE_N 32
typedef struct { int stage, room, on, n; unsigned id[ENM_LATE_N]; } EnmLateSet;
static void EnmLateReset(EnmLateSet *s, int stage, int room, int on) { s->stage = stage, s->room = room, s->on = on, s->n = 0; }
static int EnmLateHas(const EnmLateSet *s, unsigned id)
{
    int i;
    for (i = 0; i < s->n; ++i)
        if (s->id[i] == id) return 1;
    return 0;
}
static int EnmLateAdd(EnmLateSet *s, unsigned id)  // 1 added, 0 known, -1 full
{
    if (EnmLateHas(s, id)) return 0;
    if (s->n >= ENM_LATE_N) return -1;
    s->id[s->n++] = id;
    return 1;
}
static int EnmLateReady(int cls) { return cls >= 1 && cls < 0x0F; }

// ---------------------------------------------------------------- the down countdown never gets shorter from a hit
// A ghost lies down with a countdown (GhostWork +0x98, [[work+0x28]+0x98]) that GhostUpdate 0x484430 drains by the enemy
// frame time; a hit on it re-rolls it (the hit 0x47F6xx: x 0.5, then 30 or + 3, + 0..2; the down step 0x481870 again:
// 60..120 s at HP 0), which can be lower (the user's log: 62.8 s, then 118.9 s 0.8 s later; other rolls fall). The
// user's choice: a hit never shortens it, the longer of the time left and the new roll is kept. Per tick, while the ghost
// stays in class 0x0A (down with a running countdown): a fall of more than one tick's drain (dt, the enemy frame time
// [0x614FF0], <= 0.1 s) plus ENM_CD_TOL is a re-roll, and the time left (prev - dt) is written back. Returns the value to
// write, 0 = leave the game's. Other classes (the Sword's pin 0x0B, rising 8, down with no countdown 9) are never touched.
#define ENM_CD_TOL 0.2f
static int EnmCdGhostType(int type) { return (type >= 0x0B && type <= 0x0F) || type == 0x11; }
static float EnmCdKeep(int prevCls, float prevCd, int cls, float cd, float dt)
{
    float want;
    if (prevCls != 0x0A || cls != 0x0A || !(prevCd > 0.0f) || !(cd > 0.0f)) return 0.0f;
    if (!(dt > 0.0f)) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    want = prevCd - dt;
    if (!(want > 0.0f) || !(cd < want - ENM_CD_TOL)) return 0.0f;
    return want;
}
// One ghost, one gameplay tick (consecutive = it was seen on the last dispatcher tick). The Sword's pin (class 0x0B, its
// countdown (1..2) x 600 s) ends in a short down when the Sword is pulled out: the pin handler 0x481DB0 sets class 0x0A,
// and on the next tick the down step 0x481870 (step 0x63) sets 2..5 s, lower than the pin's time: that down is the game's
// (pin = 1 from a class 0x0B until the ghost is neither down nor pinned), never "kept".
typedef struct { int cls, pin; float cd; } EnmCdState;
static float EnmCdStep(EnmCdState *s, int consecutive, int cls, float cd, float dt)
{
    float keep = 0.0f;
    if (cls == 0x0B) s->pin = 1;
    else if (cls != 0x0A) s->pin = 0;
    if (consecutive && !s->pin) keep = EnmCdKeep(s->cls, s->cd, cls, cd, dt);
    s->cls = cls, s->cd = keep > 0.0f ? keep : cd;
    return keep;
}

// ================================================================ v3.38 EN338: follow for the whole world visit, spotted at any
// distance, the story ghosts follow, no Wheelchair, the game's own downs
// RE: staging/v338_feedback/RESEARCH_ghosts.md and w2_ghosts/PLAN.md (decomp: ghost_484730.c EnemyGhostArrivalCheck 0x486150 /
// EnemyGhostFollowerCreate 0x486070, en_463950.c EnemyGhostMayEnterRoom 0x464880 (gEnemyManager.denied is cleared on every
// call), en_46f520.c 0x46F9C0 (the room timer a ghost needs; -1 = never: 1.7e38), en_46de80.c the sense tests,
// evhandler_4cd420.c 0x4CE050 (Andrew's pin message), en_4b9810.c ScratchThink 0x4BFCB0 (types 0F and 11: the 30 s in 5:3)).

// ---------------------------------------------------------------- the down countdown: the game's own by default
// The user (items 9 / 10): "Allow downed ghosts to be handled by the game's code not ours". DownKeep (hidden) 0 = vanilla
// (a hit halves the countdown, then about 30 s; no one-tick dip of the revival bar); 1 = v3.35's "a hit never shortens it".
#define ENM_DOWNKEEP_DEFAULT 0

// ---------------------------------------------------------------- which ghosts follow by the ghosts' path
// the victims, and (FollowStoryGhosts, hidden, 1) the four story ghosts: Cynthia's ghost 0C v0, Jasper 0D v0, Andrew 0E,
// Richard 0F; never Walter / the Bodies (10: no ghost type), never the Patient (11: a monster, it dies)
static int EnmFollowGhost(int type, int variant, int story)
{
    return EnmIsVictim(type, variant) || (story && EnmIsStoryGhost(type, variant));
}
// Who may pursue (ENM_PUR_*): a ghost original (a ghost type with its FFFF00nn id) by the ghosts' rule (Monsters + Victims;
// its placement's mode 0 or the arrived 4); a ghost's copy as a copy (mode 0); else the monsters' rule (EnmPursuerWhy).
static int EnmPursuerWhy3(int copy, int ghostId, int victims, int story, int type, int variant, unsigned modeWord, int released,
                          int aiState, int scriptId, int killIdx)
{
    if (EnmGhostType(type) && (copy || ghostId)) {
        if (!victims || !EnmFollowGhost(type, variant, story)) return ENM_PUR_OTHER;
        if (copy) return (modeWord & 0xF) != 0 ? ENM_PUR_EVENT : ENM_PUR_YES;
        return EnmVictimModeOk(modeWord) ? ENM_PUR_YES : ENM_PUR_EVENT;
    }
    return EnmPursuerWhy(copy, 0, victims, type, modeWord, released, aiState, scriptId, killIdx);
}
// the story guards of a room change (0 = it may come): the two story writes a follower could make in a room the story did
// not plan
//  ANDREW: type 0E in stage 4 before flag 0x2DC: the end of a Sword pin of any 0E runs 0x4CE050, which there (stage 4,
//          0x2DC clear) plays the "Andrew pinned" event;
//  RICHARD: types 0F and 11 (the Patient runs the same ScratchThink 0x4BFCB0) into or out of Building World room 5:3 while
//          0x3BF is clear: 30 s there (while 0x3AE is clear) set 0x3BF and take the ghost out of the room (states 6 / 7).
enum { ENM_SF_OK, ENM_SF_ANDREW, ENM_SF_RICHARD, ENM_SF_N };
static const char *const kEnmStoryFollowWhy[ENM_SF_N] = {
    "", "Andrew in stage 4 before story flag 2DC (a pin there plays his story event)",
    "room 5:3 before story flag 3BF (30 s there set it: Richard's scripted exit)" };
static int EnmStoryFollowWhy(int type, int stage, int fromRoom, int toRoom, int flag2DC, int flag3BF)
{
    if (type == 0x0E && stage == 4 && !flag2DC) return ENM_SF_ANDREW;
    if ((type == 0x0F || type == 0x11) && stage == 5 && !flag3BF && (fromRoom == 3 || toRoom == 3)) return ENM_SF_RICHARD;
    return ENM_SF_OK;
}

// ---------------------------------------------------------------- no timing rule keeps a follower out (item 4)
// The door verdict without v3.30's 6 s list age (real doors only stay: door 0, lifts, a world change carry nobody).
static int EnmDoorVerdict2(int stage, int listStage, int listRoom, int room, int door, int elevator, int prevRoom)
{
    return EnmDoorVerdict(stage, listStage, listRoom, room, door, elevator, 0u, 0u, prevRoom);
}
// What a due follower does now: 0 wait, 1 come to the door spot area (the player has left it, >= 300 away), 2 come to a
// ring spot away from the player (10 s waited). Never "stay behind" (v3.37: 20 s): it waits until it fits.
static int EnmArriveStep2(float sinceDue, float playerFromDoor)
{
    if (playerFromDoor >= 300.0f) return 1;
    if (sinceDue >= 10.0f) return 2;
    return 0;
}
// FollowKeepDoors (hidden): missing / "Always" / < 0 = pursuits last the whole world visit (v3.38); 0 = no pursuit (v3.34);
// 1..32 = v3.35's "lost after that many doors without catching up"
#define ENM_KEEP_ALWAYS (-1)
static int EnmKeepDoorsIni2(int v) { return v < 0 ? ENM_KEEP_ALWAYS : v > ENM_FOLLOW_MAX ? ENM_FOLLOW_MAX : v; }
// the game's room timer gate of a ghost (0x486150: room timer >= 0x46F9C0; that function's "never" is 1.7e38)
enum { ENM_RT_OK, ENM_RT_WAIT, ENM_RT_NEVER };
static int EnmRoomTimerGate(float timer, float threshold)
{
    if (!(threshold < 1.0e37f)) return ENM_RT_NEVER;
    return timer >= threshold ? ENM_RT_OK : ENM_RT_WAIT;
}

// ---------------------------------------------------------------- spotted at any distance (item 6)
// A per-room-visit mark per monster: NEAR (within FollowRange 800 / 400 up or down, a flying Hummer 1100, at any tick of the
// visit: every follow monster attacks in reach, so a hit on the player is within it too), HIT (its HP fell below the
// highest seen this visit: hit by the player, Eileen or anything on her side), SAW (its EnemySense record found the player:
// found = 1, no object, target 0). Ghosts need none: every ghost in the room pursues.
enum { ENM_SEEN_NEAR = 1, ENM_SEEN_HIT = 2, ENM_SEEN_SAW = 4 };
#define ENM_SEEN_N 96  // = the enemy list walk's guard
typedef struct { unsigned id; float hpTop; int how; } EnmSeen;
typedef struct { int serial, n; EnmSeen e[ENM_SEEN_N]; } EnmSeenSet;
static void EnmSeenReset(EnmSeenSet *s, int serial) { s->serial = serial, s->n = 0; }
static EnmSeen *EnmSeenFind(EnmSeenSet *s, unsigned id)
{
    int i;
    for (i = 0; i < s->n; ++i)
        if (s->e[i].id == id) return &s->e[i];
    return 0;
}
// one tick's look at an enemy (hp < 0 or no number: unknown): the marks it has now (ENM_SEEN_*); -1 = the set is full
static int EnmSeenSee(EnmSeenSet *s, unsigned id, float hp, int isNear, int saw, int *fresh)
{
    EnmSeen *e = EnmSeenFind(s, id);
    int how = 0;
    const int hpOk = hp >= 0.0f && hp < 1.0e9f;
    if (fresh) *fresh = 0;
    if (!e) {
        if (s->n >= ENM_SEEN_N) return -1;
        e = &s->e[s->n++];
        e->id = id, e->how = 0, e->hpTop = hpOk ? hp : -1.0f;
    }
    if (isNear) how |= ENM_SEEN_NEAR;
    if (saw) how |= ENM_SEEN_SAW;
    if (hpOk) {
        if (e->hpTop >= 0.0f && hp < e->hpTop - 0.01f) how |= ENM_SEEN_HIT;
        if (hp > e->hpTop) e->hpTop = hp;
    }
    if (fresh) *fresh = how & ~e->how;
    e->how |= how;
    return e->how;
}
// the EnemySense record (0x40 bytes) in the type block [work+0x24] (0x45B690): 03 Hummer +0x44, 07 Gum Head +0x7C, 08 Twin
// Victim +0x24, 0A Sniffer Dog +0x10 (SH4Decomp include/enemy_4700c0.h, enemy_493f30.h, enemy_twinvictim.h, enemy_48bdb0.h)
static int EnmSenseOff(int type)
{
    switch (type) {
    case 0x03: return 0x44;
    case 0x07: return 0x7C;
    case 0x08: return 0x24;
    case 0x0A: return 0x10;
    }
    return 0;
}
// its last test found the player: found (+0x10) 1 with no object (+0x18; the object searches 0x46E390 / 0x46E430 set one)
// and target (+0x3C) 0 (EnemyTargetExists: 0 the player, 1 Eileen, 2 Cynthia)
static int EnmSenseSawPlayer(int found, unsigned object, int target) { return found == 1 && object == 0 && target == 0; }
// the candidates of a door, nearest first: pursuers (prio) before the newly spotted; no distance test (ok = eligible)
typedef struct { float d2; int ok, prio; } EnmCand4;
#define ENM_PUR_MAX 64  // candidates of one room (the list walk takes 64)
static int EnmPickPursuers4(const EnmCand4 *c, int n, int max, int *out)
{
    int used[ENM_PUR_MAX], m = 0, i, j, pass;
    if (max > ENM_PUR_MAX) max = ENM_PUR_MAX;
    if (n > ENM_PUR_MAX) n = ENM_PUR_MAX;
    for (i = 0; i < n; ++i) used[i] = 0;
    for (pass = 0; pass < 2; ++pass)
        while (m < max) {
            int best = -1;
            for (j = 0; j < n; ++j) {
                if (used[j] || !c[j].ok || (pass == 0) != (c[j].prio != 0)) continue;
                if (best < 0 || c[j].d2 < c[best].d2) best = j;
            }
            if (best < 0) break;
            used[best] = 1;
            out[m++] = best;
        }
    return m;
}
// The order a door fills FollowMax: the pursuers in the room you left (nearest first), the pursuers away (on their way or in
// other rooms, the oldest pursuit first), then the newly spotted ones in the room (nearest first). The rest wait: each is a
// pursuer and comes through a later door.
static void EnmDoorSlots(int roomPrio, int away, int roomNew, int max, int *a, int *b, int *c)
{
    int left = max > 0 ? max : 0;
    *a = roomPrio < left ? roomPrio : left, left -= *a;
    *b = away < left ? away : left, left -= *b;
    *c = roomNew < left ? roomNew : left;
}

// ---------------------------------------------------------------- the pursuit across doors (item 5)
// a pursuer seen in the room: a ghost's pin (0x0B: it stays with its Sword) or a monster's / copy's death ends it; down (9 /
// 0x0A, also at HP 0) keeps it: the game decides when a ghost gets up (v3.35 ended it at HP 0)
static int EnmPursueOnSight2(EnmPursuer *e, int serial, int cls, int dead)
{
    if (e->ghost && cls == 0x0B) return ENM_PURSUE_PINNED;
    if (!e->ghost && (dead || cls == 0x0B)) return ENM_PURSUE_DIED;
    e->seen = serial;
    e->standing = EnmStanding(cls);
    return ENM_PURSUE_KEEP;
}
// at a door, a pursuer away (not in the room you left): its record (recState -1 = none) says whether it is still in this
// world. A ghost's 0x0B is its pin, a monster's its death; recRoom 0x63 = moved out of the world (0x4651C0). Down or not,
// it keeps pursuing; maxMisses > 0 (FollowKeepDoors 1..32) restores v3.35's door count.
static int EnmPursueAtDoor2(int *misses, int maxMisses, int stage, int ghost, int recState, int recStage, int recRoom)
{
    if (recState <= 0 || recStage != stage || recRoom == 0x63 || recRoom <= 0) return ENM_PURSUE_GONE;
    if (recState == 0x0B) return ghost ? ENM_PURSUE_PINNED : ENM_PURSUE_DIED;
    if (maxMisses > 0 && ++*misses >= maxMisses) return ENM_PURSUE_BEHIND;
    return ENM_PURSUE_KEEP;
}

// ---------------------------------------------------------------- the space gates of an arrival (item 5)
// A follower comes in only while the room has space for it; else it waits (pending: here, or through a later door). The
// gates: the room's enemy list (40), task slots (40 kept free), actor records (20 kept free), the bone budget, hit shapes
// (24 kept free of the 356) and IK handles (64 of the 512), and for a copy whose record keeps a state a fresh copy would not
// have, one free slot of the game's 64 enemy records (its state goes in for the create and out again). The file arena (30 MB
// kept free, 16 extra files a room) is tested at the door (EnmEnsureFiles).
enum { ENM_SPACE_OK, ENM_SPACE_LIST, ENM_SPACE_TASKS, ENM_SPACE_ACTORS, ENM_SPACE_BONES, ENM_SPACE_HITS, ENM_SPACE_IK,
       ENM_SPACE_RECORD, ENM_SPACE_N };
static const char *const kEnmSpaceWhy[ENM_SPACE_N] = {
    "it fits", "the room's enemy list is full (40)", "task slots (40 kept free)", "actor records (20 kept free)",
    "the bone budget", "hit shapes (24 kept free)", "IK handles (64 kept free)",
    "no free enemy record for its kept state (64 in use)" };
#define ENM_HIT_KEEP 24
#define ENM_IK_KEEP 64
typedef struct { int listOk, tasksOk, actorsOk, bonesOk, hitFree, ikFree, recFree; } EnmSpace;  // hitFree / ikFree < 0: not known (wait)
static int EnmSpaceWhy(const EnmSpace *s, int needRec)
{
    if (!s->listOk) return ENM_SPACE_LIST;
    if (!s->tasksOk) return ENM_SPACE_TASKS;
    if (!s->actorsOk) return ENM_SPACE_ACTORS;
    if (!s->bonesOk) return ENM_SPACE_BONES;
    if (s->hitFree < ENM_HIT_KEEP) return ENM_SPACE_HITS;  // v3.38 REV338 (inspection 2): not known (< 0) = wait too
    if (s->ikFree < ENM_IK_KEEP) return ENM_SPACE_IK;
    if (needRec && s->recFree <= 0) return ENM_SPACE_RECORD;
    return ENM_SPACE_OK;
}
// a copy's record keeps a state a fresh copy would not have: anything but standing at full HP
static int EnmCopyNeedsRecord(int state, float hp, float hpMax)
{
    if (state == 0) return 0;  // (no state kept: made fresh anyway)
    return !EnmStanding(state) || !(hp >= hpMax);
}

// ---------------------------------------------------------------- no Wheelchair (item 8): the strays of v3.37
// A real Wheelchair (06) that followed in v3.37 keeps a record in a foreign room (in a save too): no longer a wanderer, it
// would hold one of the 64 records while its home placement spawns it again (gate 5). Such a record is deleted at the room
// load (one killed there keeps its kill bit: dead, as the game's own reuse of a dead record does); the train event's own
// record move (room 14 -> 22) is never one.
// v3.38 REV338 R1: a ghost that follows by the ghosts' path: a ghost type (0B..0F) with its FFFF00nn ghost id, or a copy.
// Every stock placement of 0B..0F has an id override FFFF00nn (tools/em_script_dump.py: 81 ghost placements, 0 without; the
// story ghosts Cynthia 0C v0 FFFF0002, Jasper 0D v0 FFFF0003, Andrew 0E FFFF0004, Richard 0F FFFF0005), and
// EnemyAllocWorkStruct 0x45CA70 makes the override the spawn id (info+0 = info+0x1C when non-zero), so the story ghosts'
// originals ARE ghost-id ghosts; their records arrive through EnemyGhostArrivalCheck 0x486150 (every ghost-type record,
// any id), the 0x486199 tag path. A story ghost without a ghost id (no stock script makes one: a cheat table's spawn) is
// not followed (its home placement's gate is not known for a record away) and is an anomaly.
static int EnmGhostKind(int type, int copy, unsigned id) { return EnmGhostType(type) && (copy || EnmIsGhostId(id)); }
static int EnmStoryNoGhostId(int type, int variant, int copy, unsigned id)
{
    return !copy && EnmIsStoryGhost(type, variant) && !EnmIsGhostId(id);
}
// v3.38 REV338 R2: the ghosts' arrival rule at 0x486199 for one ghost record. Untagged: the stock answer (0x464880) as it
// is. Tagged (it follows you): the game's story refusal (`denied` after a "no") ends its pursuit; on a stock "yes" and a
// stock "no" alike it comes only while the story guard allows it, the room timer has run out (0x46F9C0; else the caller's
// own test keeps it out), no other tagged ghost came in this tick and the room has space.
enum { ENM_GA_STOCK, ENM_GA_REFUSE, ENM_GA_ADMIT, ENM_GA_DENIED };
static int EnmGhostAdmit(int stockYes, int denied, int tagged, int storyOk, int timerOk, int tickFree, int spaceOk)
{
    if (!tagged) return ENM_GA_STOCK;
    if (!stockYes && denied) return ENM_GA_DENIED;
    if (!storyOk) return ENM_GA_REFUSE;
    if (!timerOk) return ENM_GA_STOCK;
    if (!tickFree || !spaceOk) return ENM_GA_REFUSE;
    return ENM_GA_ADMIT;
}
static int EnmStrayWheelchair(unsigned id, int type, int recStage, int recRoom, int stage, int half)
{
    const int home = EnmScriptRoom(id);
    if (type != 0x06 || !EnmIsScriptId(id) || EnmIsGhostId(id)) return 0;
    if (EnmScriptStage(id) != stage || EnmScriptHalf(id) != half || recStage != stage) return 0;
    if (recRoom <= 0 || recRoom == home || (home == 14 && recRoom == 22)) return 0;
    return 1;
}

// ---------------------------------------------------------------- v3.41 EN341: ghost copies come in the ghosts' way
// Why the game refused ghost copies in another room (v3.40 log: 4 of 4 in stage 2 rooms 4 / 2 / 1; SH4Decomp, all matched):
// a ghost type's create (victim 0x4AFA90, Cynthia 0x478670 file, Richard 0x4B9810 file) calls EnemyGhostReconcile 0x482590,
// then EnemySpawnGate 0x464F20. When a record of its id is in the table, the gate asks EnemyGhostMayEnterRoom 0x464880 on
// the record's own room (Refined had put this room there) and answers 1 on a "no": the create of 0B / 0C / 0F is undone
// (0D / 0E go on, without the record's state). Reconcile keeps a mode-0 record of this stage with a half chance (else it
// deletes it when not down), so a copy with its record was refused at least every second time where the rule keeps ghosts
// out (stage 2 first visit: every room but 7 / 8 / 9 / 12 / 21 and some for 0C). Refined's 0x486199 answers only the rule
// call of the arrival check 0x486150. The game's own ghosts never meet the gate: EnemyGhostFollowerCreate 0x486070
// deletes the record when it makes the placeholder, and 0x4852B0 makes the real ghost from it with no record, once the
// placeholder has regenerated to max HP (0x485D90): an arriving ghost stands at full HP. The late copies (made with no
// record) were made in those same rooms in the same log (09:40:41 room 4, 09:41:10 room 2, 09:41:18 room 1).
// EnmGhostGateRefuses: the game's answer for one ghost create (the model the test checks EnmGhostCopyMake against):
// recInTable = a record of its id is in the table, ruleYes = 0x464880 on it, keepHalf = Reconcile's draw < 0.5,
// pinned = its record state 0x0B, countdown = its record's down countdown; spawn mode 0, this stage, this room.
static int EnmGhostGateRefuses(int recInTable, int ruleYes, int keepHalf, int pinned, float countdown)
{
    if (!recInTable || ruleYes) return 0;
    if (!pinned && !keepHalf && !(countdown > 0.0f)) return 0;  // (Reconcile deleted the record: a fresh create)
    return 1;
}
// how a ghost copy is made in a room that is not its own (a door follower; a copy of the ledger coming back):
// RECORD = with its record (the rule says yes: its HP / down / pin as they were), FRESH = with no record of its id in the
// table (standing: at full HP, as the game makes an arriving ghost), LATER = not now (pinned or down where the rule says
// no: the game cannot restore that state here; it stays where it is, as a ghost record the gate refuses does),
// DENIED = a story flag's refusal (never overridden: it goes home).
enum { ENM_GCM_RECORD, ENM_GCM_FRESH, ENM_GCM_LATER, ENM_GCM_DENIED, ENM_GCM_N };
static int EnmGhostCopyMake(int ruleYes, int denied, int state, float countdown)
{
    if (ruleYes) return ENM_GCM_RECORD;
    if (denied) return ENM_GCM_DENIED;
    if (state == 0x0B || !(countdown <= 0.0f)) return ENM_GCM_LATER;  // (a NaN countdown: not now)
    return ENM_GCM_FRESH;
}
// the placement's story flags (info+0x44 needed, info+0x48 forbidden; EnemySpawnCheckStoryFlags 0x463FF0): the gate's
// last test, with or without a record. A flag number outside gStoryFlags (0x140 bytes) is not judged (-1).
#define ENM_STORY_FLAGS_N (0x140 * 8)
static int EnmPlacementFlagsOk(int need, int needSet, int forbid, int forbidSet)
{
    if (need < 0 || need >= ENM_STORY_FLAGS_N || forbid < 0 || forbid >= ENM_STORY_FLAGS_N) return -1;
    return (need == 0 || needSet) && (forbid == 0 || !forbidSet);
}

// ---------------------------------------------------------------- v3.49 EF349: Enemies Follow's values
// The row's values in gameplay.inc kGpName[GP_FOLLOW] order (the ini keeps the name, so the order is free): Off, Monsters,
// Victims Only (the user, 2026-11-25: only victim ghosts follow you through doors, monsters stay in their rooms),
// Monsters + Victims. "Victims" = the ghosts' path (the victims and, FollowStoryGhosts, the four story ghosts) with their
// copies; "monsters" = the follow monster types (EnmFollowMonster: 03 07 08 0A 11, the Twin Victims and the Patient too)
// with their copies.
enum { ENM_FOLLOW_OFF, ENM_FOLLOW_MONSTERS, ENM_FOLLOW_VICTIMS, ENM_FOLLOW_BOTH, ENM_FOLLOW_N };  // v3.49 EF349
static int EnmFollowValueOn(int m) { return m > ENM_FOLLOW_OFF && m < ENM_FOLLOW_N; }  // v3.49 EF349
static int EnmFollowTakesMonsters(int m) { return m == ENM_FOLLOW_MONSTERS || m == ENM_FOLLOW_BOTH; }  // v3.49 EF349
static int EnmFollowTakesVictims(int m) { return m == ENM_FOLLOW_VICTIMS || m == ENM_FOLLOW_BOTH; }  // v3.49 EF349
// v3.49 EF349: who may pursue by the value: as EnmPursuerWhy3, and without "monsters" (Victims Only) everything that does
// not take the ghosts' path (a ghost type with its ghost id, or a ghost's copy) is of no following kind
static int EnmPursuerWhy4(int monsters, int copy, int ghostId, int victims, int story, int type, int variant, unsigned modeWord,
                          int released, int aiState, int scriptId, int killIdx)
{
    if (!monsters && !(EnmGhostType(type) && (copy || ghostId))) return ENM_PUR_OTHER;
    return EnmPursuerWhy3(copy, ghostId, victims, story, type, variant, modeWord, released, aiState, scriptId, killIdx);
}
// v3.49 EF349: Victims Only at a room load: the monsters' copies that followed you leave the copies' ledger (each is made
// in its own room again, as with Follow Off); the ghost copies stay where they are. Returns how many left.
static int EnmCwDropMonsters(EnmCwLedger *l)
{
    int i, n = 0;
    for (i = 0; i < ENM_CW_N; ++i)
        if (l->e[i].used && !EnmGhostType(l->e[i].type)) l->e[i].used = 0, ++n;
    return n;
}

// ---------------------------------------------------------------- v3.58 EN358: the randomizer's victims follow too
// The user's v3.57 QA: "Victims can no longer pursuit me even on Relentless while Door Chase is set to Both." The long log
// (2026-11-27): Relentless only from 00:52:01, in Subway rooms 2:2 / 2:4 / 2:5, where every victim's ghost was one the
// randomizer had drawn ("n2 Sniffer Dog v0 -> victim ghost v1"); each door said "nobody follows (... N of no following
// kind)". Since v3.56 RE356 a monster placement may be made a victim's ghost (rzmon.inc); it keeps its script id
// (half << 31 | stage << 24 | room << 16 | n), no FFFF00nn ghost id, so EnmPursuerWhy4 / EnmGhostKind / pursuit_room.inc
// took it for a monster of no following kind (and Enemy Pursuit's in-room rules skipped it). It IS the game's own victim
// (EnemyVictimGhostCreate 0x4AFA90 makes it; pinned, never killed): its record (+0x20 id, +0x24 type, +0x36 stage, +0x37
// room) and the arrival check 0x486150 / Refined's 0x486199 answer are id-agnostic, so it takes the ghosts' path (the tag)
// exactly as the stock victims do. The stock victims (FFFF00nn) and the story ghosts are unchanged.
static int EnmDrawnGhost(int type, int variant, int copy, unsigned id)
{
    return !copy && EnmIsVictim(type, variant) && !EnmIsGhostId(id) && !EnmIsCopyId(id) && EnmIsScriptId(id);
}
// the ghosts' path of one enemy: a ghost type with its ghost id or a ghost's copy (EnmGhostKind), or a drawn victim
static int EnmGhostPath(int type, int variant, int copy, unsigned id)
{
    return EnmGhostKind(type, copy, id) || EnmDrawnGhost(type, variant, copy, id);
}
// The ghosts that followed you through a door (the tags) and what became of them in that room visit (one line at the next
// door: the user's QA asked for a per-door proof; v3.57 said only "2 ghost(s)" at the door and "acted 0 (followers
// queued)" in the room line, which counts the monsters' queue). seen: bit 1 = it came in as the real ghost (classes
// 1..0x0C: the game draws it, EnemyIsActive 0x45CA40), bit 2 = it was in the room's list still arriving (0 / 0x0D..0x0F:
// sliding through the walls, the placeholder 0x486030).
#define ENM_TAG_IN      1
#define ENM_TAG_ARRIVING 2
static int EnmTagSeenBits(int cls) { return cls > 0 && (cls <= 0x0C || cls == 0x12) ? ENM_TAG_IN : ENM_TAG_ARRIVING; }
typedef struct { int tagged, in, arriving, outside; } EnmTagSum;
static EnmTagSum EnmTagSummary(const unsigned *tag, const unsigned char *seen, int n)
{
    EnmTagSum s;
    int i;
    s.tagged = s.in = s.arriving = s.outside = 0;
    for (i = 0; i < n; ++i) {
        if (!tag[i]) continue;  // (a story refusal took it off: its own line said so)
        ++s.tagged;
        if (seen[i] & ENM_TAG_IN) ++s.in;
        else if (seen[i] & ENM_TAG_ARRIVING) ++s.arriving;
        else ++s.outside;
    }
    return s;
}
#endif
