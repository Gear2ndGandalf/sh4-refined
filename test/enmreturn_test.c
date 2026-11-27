// v3.56 ER356: native test of src\enmreturn_sim.h (test\build_enmreturn_test.cmd): Enemies Return Off / By Room / By
// World. 1. The pure rules: ErAction by mode x kind, ErTremerKeep, ErParse (the old On, the names, unknown text) and the
// value names against gameplay.inc's kGpName row (copied, named). 2. The rule played against a model of the game's enemy
// state (en_463950.c / en_488880.c, matched): kill bits, body records (state 0x0B) written at the room exit and by a
// save, the gate (bit = gone, body = lies dead, else stands), the hole deleting a world's records, the Tremer's coin flip
// at its room exit, saves as whole copies of the state and loads as the game applies them (gEnemyManager.stage -2), story
// rooms, kill-bit monsters (Twin Victim). Refined's two actions are the ones enemies.inc runs (EnmReviveRoom /
// ErWorldPass: a placement of a revive type in a room that is no story room loses its body and its bit).
// The user's checks: By World: saving in the world, killing, then loading must not bring them back early, and must not
// keep them dead past the world visit; By Room: a sneaky Quick Save / Quick Load brings nothing back.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/enmreturn_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// gameplay.inc kGpName[GP_REVIVE] (a copy: the row's values and the ini words, the same order as kErName)
static const char *const kGpReviveCopy[3] = { "Off", "By Room", "By World" };

static void TestRules(void)
{
    int m, k, legacy, v;
    for (m = 0; m < ER_N; ++m) CHECK(!strcmp(kErName[m], kGpReviveCopy[m]), "value %d: %s / %s", m, kErName[m], kGpReviveCopy[m]);
    for (k = 0; k < WV_N; ++k) {
        CHECK(ErAction(ER_OFF, k) == ERA_NONE, "Off does nothing (kind %d)", k);
        CHECK(ErAction(ER_ROOM, k) == (k == WV_ROOM || k == WV_ENTRY ? ERA_ROOM : ERA_NONE), "By Room, kind %d", k);
        CHECK(ErAction(ER_WORLD, k) == (k == WV_ENTRY ? ERA_WORLD : k == WV_ROOM ? ERA_DOGS : ERA_NONE), "By World, kind %d", k);
        CHECK(ErAction(-1, k) == ERA_NONE && ErAction(ER_N, k) == ERA_NONE, "a bad mode does nothing");
    }
    CHECK(ErAction(ER_ROOM, WV_LOAD) == ERA_NONE && ErAction(ER_WORLD, WV_LOAD) == ERA_NONE, "a load returns nothing");
    CHECK(ErAction(ER_WORLD, WV_OUT) == ERA_NONE && ErAction(ER_ROOM, WV_OUT) == ERA_NONE, "Room 302 returns nothing");
    CHECK(ErTremerKeep(ER_WORLD, 1, 4), "By World keeps a Tremer's body");
    CHECK(!ErTremerKeep(ER_WORLD, 0, 4) && !ErTremerKeep(ER_ROOM, 1, 4) && !ErTremerKeep(ER_OFF, 1, 4), "else the game's flip");
    CHECK(!ErTremerKeep(ER_WORLD, 1, 1) && !ErTremerKeep(ER_WORLD, 1, 9), "outside the worlds the game's flip");
    CHECK(ErParse("On", &legacy) == ER_ROOM && legacy, "v3.55's On = By Room (legacy)");
    CHECK(ErParse("on", &legacy) == ER_ROOM && legacy, "on, any case");
    CHECK(ErParse("Off", &legacy) == ER_OFF && !legacy, "Off");
    CHECK(ErParse("by room", &legacy) == ER_ROOM && !legacy, "By Room, any case");
    CHECK(ErParse("By World", &legacy) == ER_WORLD && !legacy, "By World");
    CHECK(ErParse("ByWorld", &legacy) == ER_OFF && !legacy, "unknown text = Off");
    CHECK(ErParse("", &legacy) == ER_OFF && !legacy, "empty = Off");
    CHECK(ErParse(NULL, &legacy) == ER_OFF && !legacy, "no text = Off");
    for (v = 0; v < ER_N; ++v) CHECK(ErParse(kErName[v], &legacy) == v && !legacy, "round trip %s", kErName[v]);
    CHECK(!strcmp(kErActName[ERA_WORLD], "the whole world"), "names");
}

// ---------------------------------------------------------------- the model
#define ROOMS 6     // rooms 1..5 of world 2; room 5 is a story room (an event spawn)
#define PER 3       // placements per room
enum { T_MON, T_TREMER, T_TWIN };  // a revive type with a body record; a Tremer (the coin flip); a Twin (kill bit at death)
typedef struct { int kill, type; } Place;
typedef struct { int state, stage, room; } Rec;  // state 0 none, 1 alive (left), 0x0B a body
typedef struct { unsigned bits[4]; Rec rec[ROOMS][PER]; int dead[PER]; int stage, room; } Game;  // dead[] = this room's tasks
static Place g_place[ROOMS][PER];
static Game G;
static int g_mgr;          // gEnemyManager.stage
static WvState W;
static int g_mode;
static unsigned g_rng = 1;
static int Rand100(void) { g_rng = g_rng * 1103515245u + 12345u; return (int)((g_rng >> 16) % 100); }
static int Bit(int r, int i) { const int b = r * PER + i; return (G.bits[b >> 5] >> (b & 31)) & 1; }
static void SetBit(int r, int i, int on)
{
    const int b = r * PER + i;
    if (on) G.bits[b >> 5] |= 1u << (b & 31); else G.bits[b >> 5] &= ~(1u << (b & 31));
}
static int StoryRoom(int r) { return r == 5; }
static int ReviveType(int t) { return t == T_MON || t == T_TREMER || t == T_TWIN; }
// Refined (enemies.inc EnmReviveRoom / ErWorldPass): a revive placement in a room that is no story room loses its body
// and its kill bit
static void ReviveRoom(int r)
{
    int i;
    if (StoryRoom(r) || G.stage < 2 || G.stage > 8) return;
    for (i = 0; i < PER; ++i) {
        if (!ReviveType(g_place[r][i].type)) continue;
        if (G.rec[r][i].state == 0x0B && G.rec[r][i].stage == G.stage) G.rec[r][i].state = 0;
        SetBit(r, i, 0);
    }
}
// the room exit (0x52B0E0 -> each task's destroy): a dead task writes its body (a Tremer: the coin flip), a living one
// its record
static void Exit(void)
{
    int i;
    if (G.room <= 0) return;
    for (i = 0; i < PER; ++i) {
        Rec *rc = &G.rec[G.room][i];
        if (!G.dead[i]) continue;  // (standing: as placed next time; the model keeps no alive record)
        if (g_place[G.room][i].type == T_TREMER &&
            !ErTremerKeep(g_mode, 1, G.stage) && Rand100() >= 50) {  // 0x48ACE5: !(r < 0.5)
            if (Rand100() < 20) SetBit(G.room, i, 1);  // 1 in 5: the kill bit; else nothing at all
        } else {
            rc->state = 0x0B, rc->stage = G.stage, rc->room = G.room;
        }
    }
    memset(G.dead, 0, sizeof G.dead);
}
// a room load: Refined first (EnmWrap, before the script), then the room init writes the stage, then the gate
static int Enter(int stage, int room)
{
    int i, kind, act;
    Exit();
    G.stage = stage, G.room = room;
    kind = WvStep(&W, WvRoomKind(&W, g_mgr, stage, room), stage, room);
    act = ErAction(g_mode, kind);
    if (act == ERA_ROOM) ReviveRoom(room);
    else if (act == ERA_WORLD) {
        int r;
        for (r = 1; r < ROOMS; ++r) ReviveRoom(r);
    }
    g_mgr = stage;
    for (i = 0; i < PER; ++i)  // the gate: a bit = never made; a body = lies dead; else it stands
        G.dead[i] = stage == 2 && (Bit(room, i) || G.rec[room][i].state == 0x0B);
    return kind;
}
static int Alive(int i) { return G.stage == 2 && !G.dead[i]; }
static void Kill(int i)
{
    G.dead[i] = 1;
    if (g_place[G.room][i].type == T_TWIN) SetBit(G.room, i, 1);  // 0x4C8DB0: the Twin's bit at its death
}
static void HoleOut(void)  // EnemyLeaveWorldHole(2) on the way out, then Room 302
{
    int r, i;
    Exit();
    for (r = 0; r < ROOMS; ++r)
        for (i = 0; i < PER; ++i)
            if (G.rec[r][i].stage == 2) G.rec[r][i].state = 0;
    G.room = 0;
    Enter(1, 1);
}
static Game g_save[4];
static void Save(int slot)  // EnemyManagerSave: the room's tasks write their records into the copy (not into the state)
{
    int i;
    g_save[slot] = G;
    for (i = 0; i < PER; ++i)
        if (G.dead[i] && G.room > 0) {
            g_save[slot].rec[G.room][i].state = 0x0B, g_save[slot].rec[G.room][i].stage = G.stage, g_save[slot].rec[G.room][i].room = G.room;
            g_save[slot].dead[i] = 0;
        }
    memset(g_save[slot].dead, 0, sizeof g_save[slot].dead);
}
static int Load(int slot)  // SaveSnapshotApply: the tasks are wiped (no exit records), the state is the save's, then its room
{
    const int stage = g_save[slot].stage, room = g_save[slot].room;
    memset(G.dead, 0, sizeof G.dead);
    G = g_save[slot];
    G.room = 0;
    g_mgr = WV_MGR_LOADED;
    return Enter(stage, room);
}
static void NewGame(int mode)
{
    int r, i;
    memset(&G, 0, sizeof G);
    for (r = 0; r < ROOMS; ++r)
        for (i = 0; i < PER; ++i) g_place[r][i].kill = r * PER + i, g_place[r][i].type = i == 1 && r == 3 ? T_TREMER : i == 2 && r == 4 ? T_TWIN : T_MON;
    g_mgr = WV_MGR_RESET;
    WvReset(&W);
    g_mode = mode;
    Enter(1, 1);  // Room 302
}

static void TestByWorld(void)
{
    NewGame(ER_WORLD);
    CHECK(Enter(2, 1) == WV_ENTRY, "the hole: a world entry");
    Kill(0), Kill(1);
    Enter(2, 2);
    CHECK(Enter(2, 1) == WV_ROOM && !Alive(0) && !Alive(1) && Alive(2), "back in the room: the killed lie dead");
    Save(0);  // a save in room 1 with 0 / 1 dead
    Enter(2, 2), Kill(0), Kill(2);
    Save(1);  // a save in room 2 with 0 / 2 dead
    Enter(2, 3);
    CHECK(Load(1) == WV_LOAD && !Alive(0) && !Alive(2) && Alive(1), "Quick Load in the room you killed in: they stay dead");
    Enter(2, 1);
    CHECK(!Alive(0) && !Alive(1), "room 1 after the load: still dead (the same visit)");
    Enter(2, 3), Kill(1);
    Save(2);  // kill, save, (kill again,) load
    Kill(0);
    CHECK(Load(2) == WV_LOAD && !Alive(1), "kill, save, load: the one killed before the save stays dead");
    CHECK(Alive(0), "the one killed after the save stands: the save's timeline (it was alive then: not kept dead past it)");
    Enter(2, 2);
    Enter(2, 3);
    CHECK(!Alive(1) && Alive(0), "and room 3 keeps that state at its next load (the same visit)");
    Load(0);
    Enter(2, 2);
    CHECK(Alive(0) && Alive(1) && Alive(2), "loading the earlier save (room 2 untouched then): they stand, as in that save");
    Enter(2, 1);
    CHECK(!Alive(0) && !Alive(1), "room 1 as in the earlier save: dead");
    HoleOut();
    Save(3);  // a save in Room 302 after the hole
    CHECK(Enter(2, 1) == WV_ENTRY && Alive(0) && Alive(1) && Alive(2), "a new visit: they stand again");
    Enter(2, 4), Kill(2);  // the Twin (its kill bit)
    HoleOut();
    Enter(2, 4);
    CHECK(Alive(2), "the Twin stands again at the next visit (By World clears its bit at the entry)");
    Kill(2), Kill(0);
    Save(1);
    Load(3);  // the Room 302 save (before this visit)
    CHECK(W.lastKind == WV_LOAD && G.stage == 1, "a load into Room 302");
    CHECK(Enter(2, 4) == WV_ENTRY && Alive(0) && Alive(2), "entering again from that save: a new visit");
    Load(1);
    CHECK(W.lastKind == WV_LOAD && !Alive(0) && !Alive(2), "a save made in the world loaded from Room 302: no entry, dead");
    {   // story rooms keep their dead (as By Room)
        Enter(2, 5), Kill(0);
        HoleOut();
        Enter(2, 5);
        CHECK(Alive(0), "story room: the hole deleted its body (the game's own rule)");
    }
}

static void TestTremer(void)
{
    int trial, backOff = 0, backWorld = 0, backRoomSame = 0;
    for (trial = 0; trial < 400; ++trial) {
        int mode;
        for (mode = ER_OFF; mode <= ER_WORLD; mode += 2) {  // Off and By World
            NewGame(mode);
            g_rng = 1000u + (unsigned)trial * 7u;
            Enter(2, 3), Kill(1);
            Enter(2, 2);
            Enter(2, 3);
            if (Alive(1)) mode == ER_OFF ? ++backOff : ++backWorld;
            Save(0);
            Load(0);
            if (mode == ER_WORLD) CHECK(!Alive(1), "By World: a killed Tremer stays dead through a save and its load");
            HoleOut();
            Enter(2, 3);
            if (mode == ER_WORLD) CHECK(Alive(1), "By World: the Tremer stands again at the next visit");
        }
        NewGame(ER_ROOM);
        g_rng = 5000u + (unsigned)trial;
        Enter(2, 3), Kill(1);
        Save(0);
        Load(0);
        if (Alive(1)) ++backRoomSame;
        Enter(2, 2);
        Enter(2, 3);
        CHECK(Alive(1), "By Room: the Tremer stands at the next room load");
    }
    CHECK(backWorld == 0, "By World: no Tremer came back in the visit (%d of 400)", backWorld);
    CHECK(backOff > 80 && backOff < 240, "Off: the game's own flip brings some back (%d of 400, about 40 %%)", backOff);
    CHECK(backRoomSame == 0, "By Room: a Quick Save / Quick Load in its room brings no Tremer back (%d)", backRoomSame);
}

static void TestByRoom(void)
{
    NewGame(ER_ROOM);
    Enter(2, 1), Kill(0), Kill(2);
    Save(0);  // a sneaky Quick Save in the room
    CHECK(Load(0) == WV_LOAD && !Alive(0) && !Alive(2) && Alive(1), "Quick Save + Quick Load: nothing comes back");
    Save(0);
    CHECK(Load(0) == WV_LOAD && !Alive(0), "twice: still dead");
    Enter(2, 2);
    CHECK(Enter(2, 1) == WV_ROOM && Alive(0) && Alive(2), "the room entered again: they stand");
    Kill(1);
    Enter(2, 2);
    Save(1);  // a save in room 2
    Load(1);
    CHECK(Enter(2, 1) == WV_ROOM && Alive(1), "kill, another room, save, load there, back: it stands (a room change)");
    Enter(2, 4), Kill(2);  // the Twin
    Enter(2, 3);
    Enter(2, 4);
    CHECK(Alive(2), "the Twin's kill bit is cleared at its room's load");
    Enter(2, 5), Kill(0);
    Enter(2, 4);
    Enter(2, 5);
    CHECK(!Alive(0), "story room: nothing returns there");
}

static void TestOff(void)
{
    NewGame(ER_OFF);
    Enter(2, 1), Kill(0);
    Enter(2, 2);
    Enter(2, 1);
    CHECK(!Alive(0), "Off: dead in the visit (its body)");
    HoleOut();
    Enter(2, 1);
    CHECK(Alive(0), "Off: the hole deleted the body: it stands at the next visit (the game's rule)");
    Enter(2, 4), Kill(2);
    HoleOut();
    Enter(2, 4);
    CHECK(!Alive(2), "Off: the Twin's kill bit keeps it dead for good");
}

static void TestModeChange(void)
{
    NewGame(ER_WORLD);
    Enter(2, 1), Kill(0);
    Enter(2, 2);
    g_mode = ER_ROOM;  // By World -> By Room inside the visit
    CHECK(Enter(2, 1) == WV_ROOM && Alive(0), "By Room from now: the room's dead stand at its next load");
    Kill(0);
    Enter(2, 2);
    g_mode = ER_WORLD;  // back to By World: nothing until the next entry
    Enter(2, 1);
    CHECK(!Alive(0), "By World again: dead until the next entry");
    g_mode = ER_OFF;  // Off: the game's rule from here; nothing of Refined's is left behind
    HoleOut();
    Enter(2, 1);
    CHECK(Alive(0), "Off after By World: the game's own hole rule");
    Kill(0);
    Enter(2, 2);
    g_mode = ER_WORLD;
    Enter(2, 1);
    CHECK(!Alive(0), "Off -> By World inside a visit: nothing returns before the next entry");
}

int main(void)
{
    TestRules();
    TestByWorld();
    TestTremer();
    TestByRoom();
    TestOff();
    TestModeChange();
    printf("enmreturn_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail ? 1 : 0;
}
