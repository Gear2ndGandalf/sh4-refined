// v3.56 ER356: native test of src\worldvisit_sim.h (test\build_worldvisit_test.cmd): the world-visit rule shared by
// Enemies Return By World (ER356) and the Randomizer's Reroll By World (RI356). gEnemyManager.stage as the game writes it
// (the boot's reset -1, a save's apply -2, every room init its stage) is modelled; each room load is classified the way
// EnmWrap does it (read before the room's script, which then writes the stage): room changes, world entries through
// the hole and by the story, loads in between (Quick Load, the Load Game list, a save of another world), Room 302, the
// non-world stages and a room-0 load at a world start.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/worldvisit_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// the model: gEnemyManager.stage (0xFD0BA4) and Refined's state
static int g_mgr;
static WvState S;
static void Boot(void) { g_mgr = WV_MGR_RESET; WvReset(&S); }
static void Apply(void) { g_mgr = WV_MGR_LOADED; }  // SaveSnapshotApply -> EnemyManagerLoad 0x464740
// one room load: EnmWrap reads the kind, then SceneRoomScriptRun's EnemyPersistenceInit writes the stage
static int Room(int stage, int room)
{
    const int kind = WvStep(&S, WvRoomKind(&S, g_mgr, stage, room), stage, room);
    g_mgr = stage;
    return kind;
}
static void Hole(int stage) { g_mgr = stage; }  // EnemyLeaveWorldHole writes the current stage back (no change)

static void TestClassify(void)
{
    CHECK(WvClassify(WV_MGR_LOADED, 2) == WV_LOAD, "a load into a world");
    CHECK(WvClassify(WV_MGR_LOADED, 1) == WV_LOAD, "a load into Room 302");
    CHECK(WvClassify(WV_MGR_RESET, 1) == WV_FIRST, "the boot");
    CHECK(WvClassify(WV_MGR_RESET, 4) == WV_FIRST, "the boot, a world (never seen: no entry)");
    CHECK(WvClassify(-7, 4) == WV_FIRST, "any other negative");
    CHECK(WvClassify(2, 2) == WV_ROOM, "the same world");
    CHECK(WvClassify(1, 1) == WV_ROOM, "Room 302 to Room 302");
    CHECK(WvClassify(1, 2) == WV_ENTRY, "the hole");
    CHECK(WvClassify(4, 5) == WV_ENTRY, "the story: world to world");
    CHECK(WvClassify(9, 8) == WV_ENTRY, "stage 9 back to 8");
    CHECK(WvClassify(2, 1) == WV_OUT, "out to Room 302");
    CHECK(WvClassify(8, 9) == WV_OUT, "8 to 9");
    CHECK(WvClassify(0, 10) == WV_OUT, "the title to the prologue");
    {
        int s, w = 0;
        for (s = -3; s <= 13; ++s) w += WvIsWorld(s);
        CHECK(w == 7 && WvIsWorld(2) && WvIsWorld(8) && !WvIsWorld(1) && !WvIsWorld(9), "worlds 2..8");
    }
    CHECK(WvIsRoomChange(WV_ROOM) && WvIsRoomChange(WV_ENTRY) && WvIsRoomChange(WV_OUT), "room changes");
    CHECK(!WvIsRoomChange(WV_LOAD) && !WvIsRoomChange(WV_FIRST), "a load and the first room are none");
    CHECK(!strcmp(kWvKindName[WV_LOAD], "after a load") && !strcmp(kWvKindName[WV_ENTRY], "world entry"), "names");
}

static void TestJourney(void)
{
    Boot();
    CHECK(Room(0, 1) == WV_FIRST, "the title's room (if it runs through the hook)");
    CHECK(Room(10, 1) == WV_OUT && S.world == 0, "New Game: the prologue");
    CHECK(Room(1, 1) == WV_OUT, "Room 302");
    CHECK(Room(1, 2) == WV_ROOM, "Room 302's bathroom");
    CHECK(Room(2, 1) == WV_ENTRY && S.entries == 1 && S.world == 2, "the hole into the Subway: entry 1");
    CHECK(Room(2, 4) == WV_ROOM && Room(2, 2) == WV_ROOM, "Subway rooms");
    Apply();  // Quick Load of a save made in 2:2
    CHECK(Room(2, 2) == WV_LOAD && S.entries == 1 && S.world == 2 && S.loads == 1, "Quick Load: no room change, no entry");
    CHECK(Room(2, 4) == WV_ROOM, "the next door is a room change again");
    Hole(2);
    CHECK(Room(1, 1) == WV_OUT && S.world == 0, "out through the hole");
    Apply();  // the Load Game list: a save made in world 4
    CHECK(Room(4, 7) == WV_LOAD && S.entries == 1 && S.world == 4, "a save of another world: a load, not an entry");
    CHECK(Room(4, 8) == WV_ROOM, "its rooms go on");
    CHECK(Room(5, 1) == WV_ENTRY && S.entries == 2, "the story moves you to world 5: entry 2");
    CHECK(Room(9, 2) == WV_OUT, "stage 9");
    CHECK(Room(5, 3) == WV_ENTRY && S.entries == 3, "back from stage 9: a new visit");
    Apply();
    CHECK(Room(1, 1) == WV_LOAD && S.world == 0, "a load into Room 302");
    CHECK(Room(2, 1) == WV_ENTRY && S.entries == 4, "the hole again: entry 4");
    Apply();
    Apply();  // two applies before one room (a load abandoned for another): still one load
    CHECK(Room(2, 1) == WV_LOAD && S.loads == 4, "two applies, one load");
    CHECK(S.epoch == 1 + 4 + 4, "epoch = 1 + entries + loads (%u)", S.epoch);
    CHECK(S.rooms == 13, "room changes counted (%u)", S.rooms);
}

static void TestRoomZero(void)
{
    Boot();
    Room(1, 1);
    Apply();
    CHECK(Room(2, 0) == WV_LOAD, "a room-0 load right after the apply");
    CHECK(S.loads == 0, "a room-0 load counts nothing");
    CHECK(Room(2, 4) == WV_LOAD && S.loads == 1, "the real room is the load's room (the room-0 init set the stage)");
    CHECK(Room(2, 5) == WV_ROOM, "then a room change");
    Room(1, 1);
    CHECK(Room(3, 0) == WV_ENTRY && S.entries == 0, "a room-0 load at a world start: the entry is held");
    CHECK(Room(3, 2) == WV_ENTRY && S.entries == 1, "the real room is the entry");
    CHECK(Room(3, 0) == WV_ROOM && Room(3, 3) == WV_ROOM, "a room-0 load inside the world: nothing held");
    Room(1, 1);
    Room(2, 0);       // the entry held
    Apply();
    CHECK(Room(2, 0) == WV_LOAD, "a load seen at a second room-0 load");
    CHECK(Room(2, 1) == WV_LOAD && S.entries == 1, "a load wins over a held entry");
    Apply();
    Room(1, 0);       // a held load
    CHECK(Room(2, 6) == WV_LOAD, "a held load makes any stage's next room a load");
}

static void TestFuzz(void)
{
    unsigned x = 12345u;
    int i, last = -1, loads = 0, applied = 0;
    Boot();
    for (i = 0; i < 200000; ++i) {
        int stage, kind;
        x = x * 1103515245u + 12345u;
        if ((x >> 16) % 13 == 0) {
            Apply(), applied = 1;
            continue;
        }
        stage = (int)((x >> 8) % 12);
        kind = Room(stage, 1 + (int)((x >> 20) % 30));
        CHECK(kind >= 0 && kind < WV_N, "a kind");
        if (applied) {
            CHECK(kind == WV_LOAD, "the room after an apply is always the load's (step %d)", i);
            ++loads;
        } else if (last >= 0 && last == stage) CHECK(kind == WV_ROOM, "the same stage is a room change (step %d)", i);
        else if (last >= 0 && WvIsWorld(stage)) CHECK(kind == WV_ENTRY, "another stage into a world is an entry (step %d)", i);
        else if (last >= 0) CHECK(kind == WV_OUT, "another stage outside the worlds (step %d)", i);
        applied = 0, last = stage;
        if (g_fail > 20) break;
    }
    CHECK(S.loads == (unsigned)loads, "the loads counted (%u, %d)", S.loads, loads);
}

int main(void)
{
    TestClassify();
    TestJourney();
    TestRoomZero();
    TestFuzz();
    printf("worldvisit_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
