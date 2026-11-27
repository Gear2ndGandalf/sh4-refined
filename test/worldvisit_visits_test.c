// v3.56 RI356: native test of src\worldvisit_sim.h's part 2 (the visit serials and room counts of Reroll Visits) and,
// v3.56 MERGE, of both parts together (WvRoomLoadBoth: the one step worldvisit.inc runs at every room load). Run by
// test\build_worldvisit_test.cmd after ER356's test\worldvisit_test.c (part 1). RI356's checks are kept; the merge only
// adapted how a load is marked: a load is now the game's own signal (gEnemyManager.stage -2 after a save's apply, part 1)
// instead of RI356's WvMarkLoad / the image match, so each load below is an Apply() before the room load (the model of
// 0xFD0BA4 as in worldvisit_test.c), and the names of part 2 (WvVisits, WvVisitsInit, WvVisitsReset, WVF_*).
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include "../src/worldvisit_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static WvVisits s;
static WvState k;
static int g_mgr;  // the model of gEnemyManager.stage (0xFD0BA4)
static int g_kind;
static void Init(unsigned salt) { WvVisitsInit(&s, salt); WvReset(&k); g_mgr = WV_MGR_RESET; }
static void Apply(void) { g_mgr = WV_MGR_LOADED; }  // SaveSnapshotApply -> EnemyManagerLoad
// one room load as worldvisit.inc runs it; then the room's script writes the stage (EnemyPersistenceInit)
static int Room(int stage, int room)
{
    int f;
    g_kind = WvRoomLoadBoth(&k, &s, g_mgr, stage, room, &f);
    g_mgr = stage;
    return f;
}

static void TestEntries(void)
{
    int f;
    unsigned w302, wA, wA2;
    Init(1234);
    CHECK(s.stage == -1 && s.world == 0, "nothing in force at the start");
    f = Room(1, 0);
    CHECK(f == WVF_IGNORED && s.stage == -1 && s.ignored == 1, "room 0 is ignored");
    f = Room(1, 3);  // New Game: Room 302
    CHECK((f & WVF_WORLD_ENTRY) && (f & WVF_ROOM_ENTRY) && !(f & WVF_LOAD) && s.world != 0, "the first room is a new visit");
    w302 = s.world;
    CHECK(WvRoomOf(&s, 1, 3) == 1, "its room count 1");
    f = Room(1, 4);
    CHECK(f == WVF_ROOM_ENTRY && s.world == w302 && g_kind == WV_ROOM, "a door inside the world: a room entry, the same visit");
    f = Room(2, 1);  // through the hole
    CHECK((f & WVF_WORLD_ENTRY) && s.world != w302 && s.stage == 2 && g_kind == WV_ENTRY, "into a world: a new visit");
    wA = s.world;
    Room(2, 2);
    Room(2, 1);
    CHECK(WvRoomOf(&s, 2, 1) == 2 && WvRoomOf(&s, 2, 2) == 1 && s.world == wA, "rooms count their entries; the visit stays");
    f = Room(1, 3);  // back to Room 302
    CHECK((f & WVF_WORLD_ENTRY) && s.world != wA && s.world != w302 && g_kind == WV_OUT, "back in Room 302: a new visit of it");
    f = Room(2, 1);
    wA2 = s.world;
    CHECK((f & WVF_WORLD_ENTRY) && wA2 != wA, "the world again: a new visit");
    CHECK(s.last[2] == wA2 && s.worldEntries == 4 && s.entries == 7, "the counts (%u world entries, %u room entries)",
          s.worldEntries, s.entries);
    f = Room(3, 1);  // the story moves the player to another stage (no Room 302 in between)
    CHECK((f & WVF_WORLD_ENTRY) && s.stage == 3 && g_kind == WV_ENTRY, "a cut-scene stage change is a world entry");
}

static void TestLoads(void)
{
    int f;
    unsigned wA, wB, roomCount;
    Init(77);
    Room(1, 3);
    Room(2, 1);
    Room(2, 5);
    wA = s.world;
    roomCount = WvRoomOf(&s, 2, 5);
    // a sneaky Quick Save / Quick Load in the same room: nothing moves
    Apply();
    f = Room(2, 0);  // the world start's room 0: the load waits
    CHECK(f == WVF_IGNORED && k.pending == WV_LOAD, "room 0 keeps the pending load");
    f = Room(2, 5);
    CHECK(f == WVF_LOAD && s.world == wA && WvRoomOf(&s, 2, 5) == roomCount && !k.pending && g_kind == WV_LOAD,
          "a load in the same world: no room entry, the visit goes on");
    Apply();
    f = Room(2, 5);  // the apply alone, no room 0
    CHECK(f == WVF_LOAD && WvRoomOf(&s, 2, 5) == roomCount, "the apply alone marks a load");
    f = Room(2, 6);
    CHECK(f == WVF_ROOM_ENTRY && WvRoomOf(&s, 2, 6) == 1, "the next door counts again");
    // world B, then a save of world A loaded: A's last visit comes back, no new visit
    Room(1, 3);
    Room(4, 1);
    wB = s.world;
    CHECK(wB != wA, "world B's visit");
    Apply();
    f = Room(2, 5);
    CHECK(f == (WVF_LOAD | WVF_RESUMED) && s.world == wA && s.stage == 2, "a save of world A loaded in world B: A's last visit");
    f = Room(2, 6);
    CHECK(f == WVF_ROOM_ENTRY && s.world == wA, "a door after it: the same visit of A");
    // a save of a world never visited this session (after a restart): visit 0, the same for every load
    Apply();
    f = Room(5, 2);
    CHECK(f == (WVF_LOAD | WVF_UNKNOWN) && s.world == 0 && s.stage == 5, "a world never visited: the seed's own (0)");
    Apply();
    f = Room(5, 2);
    CHECK(f == WVF_LOAD && s.world == 0, "loading it again: still 0, never a fresh draw");
    f = Room(5, 3);
    CHECK(f == WVF_ROOM_ENTRY && s.world == 0 && WvRoomOf(&s, 5, 3) == 1, "a door in it: a room entry, still the seed's own visit");
    f = Room(1, 3);
    f = Room(5, 2);
    CHECK((f & WVF_WORLD_ENTRY) && s.world != 0 && s.last[5] == s.world, "entering it through the hole: a real visit");
    CHECK(s.loads == 5 && s.resumes == 1 && s.unknowns == 1, "the load counts (%u, %u, %u)", s.loads, s.resumes, s.unknowns);
}

static void TestReset(void)
{
    int f;
    unsigned w302;
    Init(5);
    Room(1, 3);
    w302 = s.world;
    // New Game while in Room 302: the reset ends the visit, the first room is a new visit again
    WvVisitsReset(&s);
    g_mgr = WV_MGR_RESET;  // (SessionResetAll)
    f = Room(1, 3);
    CHECK((f & WVF_WORLD_ENTRY) && (f & WVF_RESET_SEEN) && s.world != w302 && g_kind == WV_FIRST, "New Game in Room 302: a new visit");
    // the title's Load: a reset, then the load's room: never an entry
    Room(2, 1);
    WvVisitsReset(&s);
    g_mgr = WV_MGR_RESET;
    Apply();
    f = Room(2, 4);
    CHECK((f & WVF_LOAD) && (f & WVF_RESUMED) && (f & WVF_RESET_SEEN) && !(f & WVF_ROOM_ENTRY) && s.world == s.last[2],
          "the title's Load: the reset, then a load (world 2's last visit)");
    Apply();
    WvVisitsReset(&s);  // a reset between the apply and the room keeps the load
    f = Room(2, 4);
    CHECK(f & WVF_LOAD, "a reset keeps a load");
}

static void TestSerials(void)
{
    unsigned a[2000];
    int i, j, dup = 0, zero = 0;
    for (i = 0; i < 2000; ++i) {
        a[i] = WvSerial(0xABCDu, (unsigned)i + 1);
        zero += a[i] == 0;
        for (j = 0; j < i; ++j) dup += a[j] == a[i];
    }
    CHECK(!zero && !dup, "2000 serials: none 0 (%d), none twice (%d)", zero, dup);
    CHECK(WvSerial(1, 1) != WvSerial(2, 1), "another session salt, another serial");
    CHECK(WvNext(0xFFFFFFFFu) == 0xFFFFFFFFu && WvNext(4) == 5, "the counts stop at the top");
    Init(0);
    CHECK(s.salt != 0, "a 0 salt is replaced");
    CHECK(WvVisitStep(&s, WV_STAGES, 1, 0) == WVF_IGNORED && WvVisitStep(&s, 1, WV_ROOMS, 0) == WVF_IGNORED &&
          WvVisitStep(&s, -1, 1, 0) == WVF_IGNORED && WvRoomOf(&s, 99, 1) == 0, "out of range: ignored");
}

// v3.56 MERGE: the two parts are one rule: on any stream of room loads and applies, a load is WV_LOAD and WVF_LOAD at
// once, a room change of the same stage WV_ROOM and a bare WVF_ROOM_ENTRY, a stage change WV_ENTRY / WV_OUT and
// WVF_WORLD_ENTRY (the first room of the session WV_FIRST with a new visit); no count of either part moves at a load.
#define BAD(c, ...) do { if (!(c)) { ++bad; CHECK(0, __VA_ARGS__); } } while (0)
static void TestBoth(void)
{
    unsigned x = 4242u;
    int i, applied = 0, last = -1, bad = 0;
    Init(99);
    for (i = 0; i < 200000 && bad < 20; ++i) {
        int stage, room, f, kind;
        unsigned e0, r0, v0;
        x = x * 1103515245u + 12345u;
        if ((x >> 16) % 11 == 0) {
            Apply(), applied = 1;
            continue;
        }
        stage = (int)((x >> 8) % 12);
        room = 1 + (int)((x >> 20) % 30);
        if (last >= 0 && (x >> 4) % 17 == 0) {  // a world start's room-0 load first (the stage already the next room's): part 2 ignores it
            BAD(Room(stage, 0) == WVF_IGNORED, "room 0 ignored by part 2 (step %d)", i);
        }
        e0 = s.entries, r0 = k.rooms, v0 = s.world;
        f = Room(stage, room);
        kind = g_kind;
        if (applied) {
            BAD(kind == WV_LOAD && (f & WVF_LOAD) && s.entries == e0 && k.rooms == r0, "a load in both (step %d: %d %x)", i, kind, f);
        } else if (last < 0) {
            BAD(kind == WV_FIRST && (f & WVF_WORLD_ENTRY), "the first room (step %d: %d %x)", i, kind, f);
        } else if (stage == last) {
            BAD(kind == WV_ROOM && f == WVF_ROOM_ENTRY && s.world == v0, "a room change in both (step %d: %d %x)", i, kind, f);
        } else {
            BAD((kind == WV_ENTRY) == WvIsWorld(stage) && (kind == WV_ENTRY || kind == WV_OUT) && (f & WVF_WORLD_ENTRY) && s.world != v0,
                "a stage change in both (step %d: %d %x)", i, kind, f);
        }
        applied = 0, last = stage;
    }
    CHECK(bad == 0, "200000 room loads / applies: both parts agree (%d disagreements)", bad);
    CHECK(s.loads == k.loads, "the loads counted alike (%u, %u)", s.loads, k.loads);
}

int main(void)
{
    TestEntries();
    TestLoads();
    TestReset();
    TestSerials();
    TestBoth();
    printf("worldvisit_visits_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
