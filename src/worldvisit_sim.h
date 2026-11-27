// v3.56 MERGE: the world-visit rule, its pure part (no game memory), ONE copy for both users: Gameplay > Enemies > Enemies
// Return By Room / By World (w4 ER356, enemies.inc) and Randomizer > Reroll Visits By Room / By World (w2 RI356,
// randomizer.inc; w3 RE356's Enemies and Spawn Site through RzVisitSalt). Both workers built a copy to the same spec
// (USER_FEEDBACK_v355.md items 8 / 16: "a sneaky Save and Load should not bypass these parameters"); the merge keeps ER356's
// load signal and kinds (part 1) and RI356's visit serials and room counts (part 2) on top of it. src/worldvisit.inc runs
// it at every room load; tests: test/worldvisit_test.c (part 1, ER356's) and test/worldvisit_visits_test.c (part 2 and
// the two parts together, RI356's adapted), test/build_worldvisit_test.cmd runs both.
//
// ---------------------------------------------------------------- THE CONTRACT
// One call per room load: worldvisit.inc WvOnRoomLoad(room), FIRST in enemies.inc EnmWrap (the re-aimed `call 52CA00` at
// 0x4F0AF8 in RoomInitLoad), before anything that reads the visit (Enemies Return's ErAtRoomLoad, the Randomizer's
// RzBeforeScript). Readers after it, on the game thread:
//   WvKind()             this room load's kind: WV_FIRST / WV_LOAD / WV_ROOM / WV_ENTRY / WV_OUT (part 1)
//   WvWorld()            the world visit in force (0 = "the seed's own": no stage change seen into this stage this session)
//   WvStageNow()         its stage (-1 = none)
//   WvRoomCount(s, r)    room r of stage s: its room entries this session (0 = none yet)
//   WvLast()             this room load's WVF_* flags (part 2)
//   WvEntries() / WvEpoch()  part 1's world entries / epochs
//   WvWorldReset()       enemies.inc EnmPoolAtReset (New Game, the title's Load): part 2's visit in force ends
//
// What counts
//  - A LOAD (Quick Load, the Extra Menu's / pause's Load Game list, the title's Load, Continue after a death) is never a
//    room change and never a world entry: the room load right after a save image was applied is WV_LOAD / WVF_LOAD,
//    whatever stage the save was made in and whatever world you were in before. No count moves.
//  - A ROOM CHANGE (WV_ROOM, WVF_ROOM_ENTRY): a room load of the same stage as the last one (a door, a lift, the train, an
//    event that moves you inside the world). The room's entry count goes up by one.
//  - A STAGE CHANGE that is not a load (a room entry into another stage than the last one): through Room 302's hole, back
//    to Room 302, the story moving the player (a cut-scene world change), stages 9 / 10 / 11. Part 1 calls it WV_ENTRY
//    when the new stage is a world (2..8) and WV_OUT otherwise (Room 302, the non-world stages: Enemies Return has nothing
//    to bring back there); part 2 starts a new visit serial for any stage (WVF_WORLD_ENTRY: Room 302 is a "world" of its own
//    for the Randomizer, so its Room 302 Items draw anew at each return under By World). Both are room changes.
//  - The first room load of the session (the boot / a new session: the enemy manager's -1): WV_FIRST for part 1 (nothing
//    to bring back, the game's records are fresh); part 2 starts its first visit there (WVF_WORLD_ENTRY).
//  - Room 0 and stages / rooms out of range: never the real room. What it saw (a load, an entry) is held for the next real
//    room load (part 1's pending; part 2 ignores the room).
//  - A save of another stage loaded (part 2): that stage's last visit this session comes back (WVF_RESUMED); a stage with
//    no visit this session yet (a restart, then a load) gets visit 0, "the seed's own" (WVF_UNKNOWN): every load of every
//    save there gets the same one, never a fresh draw.
//
// The load signal (the merge's choice: the game's own, ER356's): gEnemyManager.stage 0xFD0BA4 read BEFORE the room's
// script (EnmWrap, ahead of the re-aimed call 0x52CA00): -2 = a save image was applied since the last room init. Proven
// by the code (SH4Decomp, matched, and the stock exe md5 15a85581, every rel32 call and every [0xFD0BA4] reference):
//   - every load path sets gGame.startMode [0xFD5A74] (title Load 1 / Continue 2: title_527a50.c; the in-game Load Game
//     list 1: save_56f4e0.c after SessionResetAll; Refined's Quick Load 1: quicksave.inc), then the world start
//     func_004FA4B0 step 0 calls SaveSnapshotApply 0x4FC5B0 (its only caller: 0x4FA4DB) whenever startMode != 0;
//   - SaveSnapshotApply always calls EnemyManagerLoad 0x464740 (its only caller: 0x4FC6ED), which writes -2 at 0x464764
//     before anything else it does;
//   - the other writers of 0xFD0BA4: 0x463989 (-1, func_00463950 from SessionResetAll 0x4F53F0 via 0x4628F0: the boot,
//     a new session and the Load Game list, each BEFORE that path's apply), 0x463C5F (the stage, EnemyPersistenceInit
//     0x463C40 = the first call inside 0x52CA00: AFTER our read), 0x4651CA / 0x4652D5 (EnemyLeaveWorldHole 0x4651C0, a
//     hole event inside a room). Nothing in Refined writes it.
// So the -2 is seen at the first room load after every load, and only then. RI356's three signals (the startMode watch in
// EnmLoadWatch / the save's apply hook, Refined's quick-load flag g_qsLoading, a match of the room with the Continue
// image) each mark a load that goes through that same apply: they add no case, and are left out (no cross-check either:
// the image match could also see a door into the image's room before any room frame ran).
// Saves: nothing of this goes into a save or a side file; the counts live in memory only (after a restart a loaded save
// shows "the seed's own" visit until the next stage change, part 2; part 1 needs none: a load is WV_LOAD).
#ifndef WORLDVISIT_SIM_H
#define WORLDVISIT_SIM_H
#include <string.h>

// ================================================================ part 1 (ER356): the load signal and the room load's kind
enum { WV_FIRST, WV_LOAD, WV_ROOM, WV_ENTRY, WV_OUT, WV_N };
static const char *const kWvKindName[WV_N] = { "first room", "after a load", "room change", "world entry", "left the world" };
#define WV_MGR_RESET  (-1)   // gEnemyManager.stage after the boot's reset 0x463950
#define WV_MGR_LOADED (-2)   // gEnemyManager.stage after a save's apply (EnemyManagerLoad 0x464740)

static int WvIsWorld(int stage) { return stage >= 2 && stage <= 8; }

// mgrStage = gEnemyManager.stage read before the room's script (its last room init's stage, or -1 / -2); stage = the
// stage of the room being loaded (gGame.stage 0xFD5A90)
static int WvClassify(int mgrStage, int stage)
{
    if (mgrStage == WV_MGR_LOADED) return WV_LOAD;
    if (mgrStage < 0) return WV_FIRST;
    if (mgrStage == stage) return WV_ROOM;
    return WvIsWorld(stage) ? WV_ENTRY : WV_OUT;
}
// a real room change (the room was entered, not loaded from a save)
static int WvIsRoomChange(int kind) { return kind == WV_ROOM || kind == WV_ENTRY || kind == WV_OUT; }

// the session's view (never saved): world entries so far, the epoch (+1 at every world entry and every load: the
// per-visit state a feature keeps in memory belongs to that epoch), the world of the visit (0 = none), the counts
typedef struct { unsigned entries, epoch, loads, rooms; int world, lastKind, lastStage, lastRoom, pending; } WvState;
static void WvReset(WvState *s)
{
    s->entries = 0, s->epoch = 1, s->loads = 0, s->rooms = 0;
    s->world = 0, s->lastKind = WV_FIRST, s->lastStage = -1, s->lastRoom = -1, s->pending = 0;
}
// the kind of one room load (stage, room) by gEnemyManager.stage read before its script. A room load with room <= 0
// (no room: the world start's, if one ever runs through the hook; none seen in the logs) is never the real room: what it
// saw (a load, a world entry, leaving a world) stays pending for the next real room: a pending load makes it WV_LOAD
// whatever its stage; a pending entry / exit stands for the real room's own WV_ROOM (the room-0 init set the stage).
static int WvRoomKind(WvState *s, int mgrStage, int stage, int room)
{
    const int kind = WvClassify(mgrStage, stage), held = s->pending;
    if (room <= 0) {
        if (kind == WV_LOAD || (kind != WV_ROOM && kind != WV_FIRST && held != WV_LOAD)) s->pending = kind;
        return kind;
    }
    s->pending = 0;
    if (held == WV_LOAD) return WV_LOAD;
    if (held && kind == WV_ROOM) return held;
    return kind;
}
// one room load: its kind (WvRoomKind) and where it is; returns the kind (a room <= 0 counts nothing: its kind is pending)
static int WvStep(WvState *s, int kind, int stage, int room)
{
    if (kind < 0 || kind >= WV_N) kind = WV_FIRST;
    if (room > 0) {
        if (kind == WV_ENTRY) ++s->entries, ++s->epoch;
        else if (kind == WV_LOAD) ++s->loads, ++s->epoch;
        if (WvIsRoomChange(kind)) ++s->rooms;
    }
    s->world = WvIsWorld(stage) ? stage : 0;  // (a load into a world: the save's world visit goes on)
    s->lastKind = kind, s->lastStage = stage, s->lastRoom = room;
    return kind;
}

// ================================================================ part 2 (RI356): the visit serials and the room counts
#define WV_STAGES 16
#define WV_ROOMS 256
enum {
    WVF_IGNORED = 1,       // room 0 / out of range: nothing changed
    WVF_ROOM_ENTRY = 2,    // a room entry (the room's count went up)
    WVF_WORLD_ENTRY = 4,   // a new visit began (with WVF_ROOM_ENTRY): a stage change that is not a load
    WVF_LOAD = 8,          // the room load of a load: no count moved
    WVF_RESUMED = 16,      // (with WVF_LOAD) another stage's last visit of this session came back
    WVF_UNKNOWN = 32,      // (with WVF_LOAD) another stage, no visit this session yet: visit 0
    WVF_RESET_SEEN = 64    // a world reset came before this room load
};
typedef struct {
    unsigned salt;                          // the session's salt (worldvisit.inc rolls it at the start)
    unsigned worlds;                        // visits begun this session
    int stage;                              // the stage of the visit in force (-1 = none)
    unsigned world;                         // the visit in force (0 = the seed's own / none)
    unsigned last[WV_STAGES];               // each stage's last visit this session (0 = none)
    unsigned room[WV_STAGES][WV_ROOMS];     // each room's entries this session
    int resetSeen;                          // a world reset since the last real room load
    int lastFlags, lastStage, lastRoom;     // the last room load (WvLast)
    unsigned entries, worldEntries, loads, resumes, unknowns, resets, ignored;  // the session's counts
} WvVisits;

static unsigned WvMix(unsigned h)  // lowbias32 (a bijection: distinct inputs give distinct outputs)
{
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}
// the n-th visit of a session (n >= 1): never 0; distinct n give distinct serials (one value in 2^32 is moved)
static unsigned WvSerial(unsigned salt, unsigned n)
{
    const unsigned s = WvMix(salt + n * 0x9E3779B9u);
    return s ? s : 0x6A09E667u;
}
static unsigned WvNext(unsigned k) { return k == 0xFFFFFFFFu ? k : k + 1; }
static void WvVisitsInit(WvVisits *s, unsigned salt)
{
    memset(s, 0, sizeof *s);
    s->salt = salt ? salt : 0x57564953u;
    s->stage = -1;
    s->lastStage = -1;
}
static void WvVisitsReset(WvVisits *s)  // a world reset: the visit in force ends (counts and last visits stay)
{
    s->stage = -1;
    s->world = 0;
    s->resetSeen = 1;
    ++s->resets;
}
// one room load; isLoad = part 1 called it WV_LOAD. Returns WVF_* flags.
static int WvVisitStep(WvVisits *s, int stage, int room, int isLoad)
{
    int f = 0;
    if (stage < 0 || stage >= WV_STAGES || room <= 0 || room >= WV_ROOMS) {
        ++s->ignored;
        return WVF_IGNORED;  // (lastFlags kept: the readers ask about real rooms)
    }
    if (s->resetSeen) f |= WVF_RESET_SEEN;
    s->resetSeen = 0;
    if (isLoad) {
        f |= WVF_LOAD;
        ++s->loads;
        if (stage != s->stage) {
            s->stage = stage;
            s->world = s->last[stage];
            if (s->world) f |= WVF_RESUMED, ++s->resumes;
            else f |= WVF_UNKNOWN, ++s->unknowns;
        }
    } else {
        f |= WVF_ROOM_ENTRY;
        ++s->entries;
        if (stage != s->stage) {
            s->worlds = WvNext(s->worlds);
            s->world = WvSerial(s->salt, s->worlds);
            s->last[stage] = s->world;
            s->stage = stage;
            f |= WVF_WORLD_ENTRY;
            ++s->worldEntries;
        }
        s->room[stage][room] = WvNext(s->room[stage][room]);
    }
    s->lastFlags = f, s->lastStage = stage, s->lastRoom = room;
    return f;
}
static unsigned WvRoomOf(const WvVisits *s, int stage, int room)
{
    return stage >= 0 && stage < WV_STAGES && room > 0 && room < WV_ROOMS ? s->room[stage][room] : 0;
}

// ================================================================ both parts: the one step of a room load (worldvisit.inc)
// mgrStage = gEnemyManager.stage before the room's script; returns part 1's kind, *flags = part 2's WVF_* flags
static int WvRoomLoadBoth(WvState *k, WvVisits *v, int mgrStage, int stage, int room, int *flags)
{
    const int kind = WvStep(k, WvRoomKind(k, mgrStage, stage, room), stage, room);
    const int f = WvVisitStep(v, stage, room, kind == WV_LOAD);
    if (flags) *flags = f;
    return kind;
}
#endif
