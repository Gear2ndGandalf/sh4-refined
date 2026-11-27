// v3.56 ER356 (staging/v356_feedback/w4_return/PLAN.md; the user's v3.55 improvement 8): Enemies Return Off / By Room /
// By World, the pure part. enemies.inc applies it (EnmWrap, the room load; the Tremer site 0x48ACE5); native test:
// test/enmreturn_test.c (test/build_enmreturn_test.cmd), which also plays the rule against a small model of the game's
// enemy state (kill bits, body records, the hole, saves and loads).
//
// How the game keeps a killed monster dead (en_463950.c, matched; enemies_sim.h "Revive Enemies"): its body record
// (state 0x0B, written by the type's destroy at the room exit and by the save) until the world's hole 0x4651C0 deletes
// the world's records on the way out (stages 2..6; the Hospital 7 and stage 8 have no hole call), and its kill bit
// (Twin Victims at the death, 1 Tremer death in 10, the record pool's reuse of an old body 0x464060) for good. A killed
// Tremer leaves no record 4 times in 10 (0x48ACB0) and stands again at the next visit of its room.
//   Off      = that rule, untouched.
//   By Room  = v3.30 - v3.55's On: at a room change (worldvisit_sim.h) the room's killed monsters of the revive types
//              lose their body record and kill bit before its script (and its killed copies leave the dead ring).
//              A load's room load is not a room change: nothing returns there (v3.55 revived at a Quick Load).
//   By World = at a world entry every killed monster of the revive types of the world (every room that is no story
//              room) loses its body record and kill bit; inside the visit the game's own bodies keep them dead, and a
//              killed Tremer always keeps its body (0x48ACE5: the destroy's coin flip answers "keep the record"), so
//              none comes back early and a save holds them as they are. A load is no entry: the save's own bodies
//              and bits are the world visit's state. The released event dogs of Subway room 2:2 stand again awake at
//              their room's first load of the visit (a session mark; after a load in between, the game's own rule).
#ifndef ENMRETURN_SIM_H
#define ENMRETURN_SIM_H
#include <string.h>
#include "worldvisit_sim.h"

enum { ER_OFF, ER_ROOM, ER_WORLD, ER_N };
static const char *const kErName[ER_N] = { "Off", "By Room", "By World" };  // [Gameplay] EnemyRevive (the ini key stays)

// what a room load does (EnmWrap, before the room's script)
enum { ERA_NONE, ERA_ROOM, ERA_WORLD, ERA_DOGS, ERA_N };
static const char *const kErActName[ERA_N] = { "nothing", "this room", "the whole world", "this room's released event dogs" };
static int ErAction(int mode, int kind)
{
    if (mode == ER_ROOM) return kind == WV_ROOM || kind == WV_ENTRY ? ERA_ROOM : ERA_NONE;
    if (mode == ER_WORLD) return kind == WV_ENTRY ? ERA_WORLD : kind == WV_ROOM ? ERA_DOGS : ERA_NONE;
    return ERA_NONE;
}
// a dead Tremer's destroy keeps its body record (By World only; else the game's own coin flip)
static int ErTremerKeep(int mode, int live, int stage) { return live && mode == ER_WORLD && WvIsWorld(stage); }

// [Gameplay] EnemyRevive's text -> the value; *legacy = 1 for v3.30 - v3.55's "On" (read as By Room, written back once)
static int ErParse(const char *s, int *legacy)
{
    int v;
    *legacy = 0;
    if (!s) return ER_OFF;
    for (v = 0; v < ER_N; ++v)
        if (!_stricmp(s, kErName[v])) return v;
    if (!_stricmp(s, "On")) return *legacy = 1, ER_ROOM;
    return ER_OFF;  // unknown text = Off (as every [Gameplay] row)
}

#endif
